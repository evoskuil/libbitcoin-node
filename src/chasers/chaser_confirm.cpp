/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/chasers/chaser_confirm.hpp>

#include <ranges>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_confirm

using namespace system;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

chaser_confirm::chaser_confirm(full_node& node) NOEXCEPT
  : chaser(node)
{
}

code chaser_confirm::start() NOEXCEPT
{
    const auto& query = archive();
    set_position(query.get_fork());
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_confirm::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating query during suspension.
    if (suspended())
        return true;

    switch (event_)
    {
        ////case chase::blocks:
        ////{
        ////    BC_ASSERT(std::holds_alternative<height_t>(value));
        ////
        ////    // TODO: value is branch point.
        ////    POST(do_validated, std::get<height_t>(value));
        ////    break;
        ////}
        case chase::resume:
        case chase::start:
        case chase::bump:
        {
            POST(do_bump, height_t{});
            break;
        }
        case chase::valid:
        {
            // value is validated block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_validated, std::get<height_t>(value));
            break;
        }
        case chase::regressed:
        case chase::disorganized:
        {
            // value is regression branch_point.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_regressed, std::get<height_t>(value));
            break;
        }
        case chase::stop:
        {
            return false;
        }
        default:
        {
            break;
        }
    }

    return true;
}

// Track validation
// ----------------------------------------------------------------------------

void chaser_confirm::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (branch_point >= position())
        return;

    set_position(branch_point);
}

void chaser_confirm::do_validated(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Cannot confirm next block until previous block is confirmed.
    if (height == add1(position()))
        do_bumped(height);
}

void chaser_confirm::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // Only necessary when bumping as next position may not be validated.
    const auto height = add1(position());
    const auto link = query.to_candidate(height);
    const auto ec = query.get_block_state(link);

    // First block state must be valid or confirmable. This is assured in
    // do_checked by chasing block checks. However bypassed blocks are not
    // marked with state, so that must be checked if not valid or confirmable.
    const auto ready =
        (ec == database::error::block_valid) ||
        (ec == database::error::block_confirmable) ||
        ((is_under_checkpoint(height) || query.is_milestone(link)) && 
            query.is_filtered(link));

    if (ready)
        do_bumped(height);
}

// Confirm (not cancellable)
// ----------------------------------------------------------------------------

// Compute relative work, set fork and fork_point, and invoke reorganize.
void chaser_confirm::do_bumped(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    if (closed())
        return;

    // If empty height is not on a candidate fork (may have been reorganized).
    auto fork = query.get_candidate_fork(height);
    if (fork.empty())
        return;

    uint256_t work{};
    if (!query.get_work(work, fork))
    {
        fault(error::confirm1);
        return;
    }

    bool strong{};
    const auto fork_point = height - fork.size();
    if (!query.get_strong(strong, work, fork_point))
    {
        fault(error::confirm2);
        return;
    }

    // Fork does not have more work than the confirmed branch. Position moves
    // up to accumulate blocks until sufficient work, or regression resets it.
    if (!strong)
    {
        set_position(height);
        return;
    }

    reorganize(fork, fork_point);
    set_position(height);
}

// Pop confirmed chain from top down to above fork point, save popped.
void chaser_confirm::reorganize(header_links& fork, size_t fork_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    auto height = query.get_top_confirmed();
    if (height < fork_point)
    {
        fault(error::confirm3);
        return;
    }

    header_links popped{};
    while (height > fork_point)
    {
        const auto link = query.to_confirmed(height);
        if (link.is_terminal())
        {
            fault(error::confirm4);
            return;
        }

        popped.push_back(link);
        if (!set_reorganized(link, height--))
        {
            fault(error::confirm5);
            return;
        }
    }

    // Top is now fork_point.
    organize(fork, popped, fork_point);
}

// Push candidates (fork) from above fork point to confirmed chain.
// Restore popped from fork point if any candidate fails to confirm.
// Fork is always shortest candidate chain stronger than confirmed chain.
// No bump required upon complete since fully stranded (no message loss).
void chaser_confirm::organize(header_links& fork, const header_links& popped,
    size_t fork_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();
    auto height = add1(fork_point);

    while (!fork.empty())
    {
        // Given height-based iteration, any block state may be enountered.
        // But unassociated should not be encounterable here once interlocked.
        const auto& link = fork.back();
        const auto ec = query.get_block_state(link);
        if (ec == database::error::unassociated)
            return;

        const auto bypass = is_under_checkpoint(height) ||
            query.is_milestone(link);

        if (bypass)
        {
            if (!query.set_filter_head(link))
            {
                fault(error::confirm6);
                return;
            }

            complete_block(error::success, link, height, bypass);
        }
        else switch (ec.value())
        {
            case database::error::block_valid:
            {
                if (!confirm_block(link, height, popped, fork_point)) return;
                break;
            }
            case database::error::block_confirmable:
            {
                // Previously confirmable is NOT considered bypass.
                complete_block(error::success, link, height, bypass);
                break;
            }
            default:
            {
                fault(error::confirm7);
                return;
            }
        }

        // After set_block_confirmable.
        if (!set_organized(link, height++))
        {
            fault(error::confirm8);
            return;
        }

        fork.pop_back();
    }

    // Prevent stall by posting internal event, avoiding external handlers.
    // Posts new work, preventing recursion and releasing reorganization lock.
    handle_event(error::success, chase::bump, height_t{});
}

bool chaser_confirm::confirm_block(const header_link& link,
    size_t height, const header_links& popped, size_t fork_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    if (const auto ec = query.block_confirmable(link))
    {
        if (!query.set_unstrong(link))
        {
            fault(error::confirm9);
            return false;
        }

        if (!query.set_block_unconfirmable(link))
        {
            fault(error::confirm10);
            return false;
        }

        if (!roll_back(popped, fork_point, sub1(height)))
        {
            fault(error::confirm11);
            return false;
        }

        complete_block(ec, link, height, false);
        return false;
    }

    // Before set_block_confirmable.
    if (!query.set_filter_head(link))
    {
        fault(error::confirm12);
        return false;
    }

    if (!query.set_block_confirmable(link))
    {
        fault(error::confirm13);
        return false;
    }

    complete_block(error::success, link, height, false);
    return true;
}

void chaser_confirm::complete_block(const code& ec, const header_link& link,
    size_t height, bool) NOEXCEPT
{
    if (ec)
    {
        // Database errors are fatal.
        if (database::error::error_category::contains(ec))
        {
            LOGF("Fault confirming [" << height << "] " << ec.message());
            fault(ec);
            return;
        }

        // UNCONFIRMABLE BLOCK (not a fault)
        notify(ec, chase::unconfirmable, link);
        fire(events::block_unconfirmable, height);
        LOGR("Unconfirmable block [" << height << "] " << ec.message());
        return;
    }

    // CONFIRMABLE BLOCK (bypass not differentiated)
    notify(error::success, chase::confirmable, height);
    fire(events::block_confirmed, height);
    LOGV("Block confirmable: " << height);
}

// private
// ----------------------------------------------------------------------------
// Checkpointed blocks are set strong by archiver, and cannot be reorganized.

bool chaser_confirm::set_reorganized(const header_link& link,
    height_t confirmed_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // Checkpointed blocks cannot be reorganized.
    BC_ASSERT(!is_under_checkpoint(confirmed_height));
    if (!query.pop_confirmed())
        return false;

    notify(error::success, chase::reorganized, link);
    fire(events::block_reorganized, confirmed_height);
    LOGV("Block reorganized: " << confirmed_height);
    return true;
}

bool chaser_confirm::set_organized(const header_link& link,
    height_t confirmed_height) NOEXCEPT
{
    BC_ASSERT(stranded());
    auto& query = archive();

    // Checkpointed blocks are set strong by archiver.
    if (!query.push_confirmed(link, !is_under_checkpoint(confirmed_height)))
        return false;

    notify(error::success, chase::organized, link);
    fire(events::block_organized, confirmed_height);
    LOGV("Block organized: " << confirmed_height);
    return true;
}

// Rollback to the fork point, then forward through previously popped.
bool chaser_confirm::roll_back(const header_links& popped, size_t fork_point,
    size_t top) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();
    for (auto height = top; height > fork_point; --height)
        if (!set_reorganized(query.to_confirmed(height), height))
            return false;

    for (const auto& fk: std::views::reverse(popped))
        if (!set_organized(fk, ++fork_point))
            return false;

    return true;
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
