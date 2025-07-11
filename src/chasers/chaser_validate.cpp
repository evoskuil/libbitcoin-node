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
#include <bitcoin/node/chasers/chaser_validate.hpp>

#include <atomic>
#include <bitcoin/system.hpp>
#include <bitcoin/node/chasers/chaser.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/full_node.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace database;
using namespace std::placeholders;

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Independent threadpool and strand (base class strand uses network pool).
chaser_validate::chaser_validate(full_node& node) NOEXCEPT
  : chaser(node),
    threadpool_(node.config().node.threads_(), node.config().node.priority_()),
    independent_strand_(threadpool_.service().get_executor()),
    subsidy_interval_(node.config().bitcoin.subsidy_interval_blocks),
    initial_subsidy_(node.config().bitcoin.initial_subsidy()),
    maximum_backlog_(node.config().node.maximum_concurrency_()),
    node_witness_(node.config().network.witness_node()),
    filter_(node.archive().filter_enabled())
{
}

code chaser_validate::start() NOEXCEPT
{
    if (!config().node.headers_first)
        return error::success;

    const auto& query = archive();
    set_position(query.get_fork());
    SUBSCRIBE_EVENTS(handle_event, _1, _2, _3);
    return error::success;
}

bool chaser_validate::handle_event(const code&, chase event_,
    event_value value) NOEXCEPT
{
    if (closed())
        return false;

    // Stop generating query during suspension.
    if (suspended())
        return true;

    switch (event_)
    {
        case chase::resume:
        case chase::start:
        case chase::bump:
        {
            POST(do_bump, height_t{});
            break;
        }
        case chase::checked:
        {
            // value is checked block height.
            BC_ASSERT(std::holds_alternative<height_t>(value));
            POST(do_checked, std::get<height_t>(value));
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

// Track downloaded
// ----------------------------------------------------------------------------

void chaser_validate::do_regressed(height_t branch_point) NOEXCEPT
{
    BC_ASSERT(stranded());
    if (branch_point >= position())
        return;

    set_position(branch_point);
}

void chaser_validate::do_checked(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());

    // Cannot validate next block until all previous blocks are archived.
    if (height == add1(position()))
        do_bumped(height);
}

void chaser_validate::do_bump(height_t) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // TODO: make store query?

    // Only necessary when bumping as next position may not be associated.
    const auto height = add1(position());
    const auto link = query.to_candidate(height);
    const auto ec = query.get_block_state(link);

    // First block state should be unvalidated, valid, or confirmable. This is
    // assured in do_checked by chasing block checks.
    const auto ready =
        (ec == database::error::unvalidated) ||
        (ec == database::error::block_valid) ||
        (ec == database::error::block_confirmable);

    if (ready)
        do_bumped(height);
}

// Validate (cancellable)
// ----------------------------------------------------------------------------

void chaser_validate::do_bumped(height_t height) NOEXCEPT
{
    BC_ASSERT(stranded());
    const auto& query = archive();

    // Bypass until next event if validation backlog is full.
    while ((backlog_ < maximum_backlog_) && !closed() && !suspended())
    {
        const auto link = query.to_candidate(height);
        const auto ec = query.get_block_state(link);

        // Must exit on unassociated so they are not set valid in bypass.
        // Given height-based iteration, any block state may be enountered.
        if (ec == database::error::unassociated)
            return;

        const auto bypass = is_under_checkpoint(height) ||
            query.is_milestone(link);

        if (bypass)
        {
            if (filter_)
            {
                post_block(link, bypass);
            }
            else
            {
                complete_block(error::success, link, height, true);
            }
        }
        else switch (ec.value())
        {
            case database::error::unvalidated:
            {
                post_block(link, bypass);
                break;
            }
            case database::error::block_valid:
            {
                if (query.is_prevouts_cached(link))
                    post_block(link, true);
                else
                    complete_block(error::success, link, height, true);
                break;
            }
            case database::error::block_confirmable:
            {
                complete_block(error::success, link, height, true);
                break;
            }
            case database::error::block_unconfirmable:
            {
                return;
            }
            ////case database::error::unassociated
            default:
            {
                fault(error::validate1);
                return;
            }
        }

        // All posted validations must complete or this is invalid.
        // So posted validations continue despite network suspension.
        set_position(height++);
    }
}

void chaser_validate::post_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    BC_ASSERT(stranded());
    backlog_.fetch_add(one, std::memory_order_relaxed);
    PARALLEL(validate_block, link, bypass);
}

// Unstranded (concurrent by block)
// ----------------------------------------------------------------------------

void chaser_validate::validate_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    if (closed())
        return;

    code ec{};
    chain::context ctx{};
    auto& query = archive();

    // TODO: implement allocator parameter resulting in full allocation to
    // shared_ptr<block>, to optimize deallocate (12% of milestone/filter).
    const auto block = query.get_block(link, node_witness_);

    if (!block)
    {
        ec = error::validate2;
    }
    else if (!query.get_context(ctx, link))
    {
        ec = error::validate3;
    }
    else if ((ec = populate(bypass, *block, ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate4;
    }
    else if ((ec = validate(bypass, *block, link, ctx)))
    {
        if (!query.set_block_unconfirmable(link))
            ec = error::validate5;
    }

    complete_block(ec, link, ctx.height, bypass);

    // Prevent stall by posting internal event, avoiding external handlers.
    if (is_one(backlog_.fetch_sub(one, std::memory_order_relaxed)))
        handle_event(error::success, chase::bump, height_t{});
}

code chaser_validate::populate(bool bypass, const chain::block& block,
    const chain::context& ctx) NOEXCEPT
{
    const auto& query = archive();

    if (bypass)
    {
        block.populate();
        if (!query.populate_without_metadata(block))
            return system::error::missing_previous_output;
    }
    else
    {
        // Internal maturity and time locks are verified here because they are
        // the only necessary confirmation checks for internal spends.
        if (const auto ec = block.populate_with_metadata(ctx))
            return ec;

        // Metadata identifies internal spends alowing confirmation bypass.
        if (!query.populate_with_metadata(block))
            return system::error::missing_previous_output;
    }
    
    return error::success;
}

code chaser_validate::validate(bool bypass, const chain::block& block,
    const database::header_link& link, const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();

    if (!bypass)
    {
        code ec{};
        if ((ec = block.accept(ctx, subsidy_interval_, initial_subsidy_)))
            return ec;

        if ((ec = block.connect(ctx)))
            return ec;

        if (!query.set_prevouts(link, block))
            return error::validate6;
    }

    if (!query.set_filter_body(link, block))
        return error::validate7;

    // After set_prevouts and set_filter_body.
    if (!bypass && !query.set_block_valid(link, block.fees()))
        return error::validate8;

    return error::success;
}

// May be either concurrent or stranded.
void chaser_validate::complete_block(const code& ec, const header_link& link,
    size_t height, bool bypass) NOEXCEPT
{
    if (ec)
    {
        // Node errors are fatal.
        if (node::error::error_category::contains(ec))
        {
            LOGF("Fault validating [" << height << "] " << ec.message());
            fault(ec);
            return;
        }

        // INVALID BLOCK (not a fault)
        notify(ec, chase::unvalid, link);
        fire(events::block_unconfirmable, height);
        LOGR("Invalid block [" << height << "] " << ec.message());
        return;
    }

    // VALID BLOCK
    notify(ec, chase::valid, possible_wide_cast<height_t>(height));
    fire(events::block_validated, height);
    LOGV("Block validated: " << height << (bypass ? " (bypass)" : ""));
}

// Overrides due to independent priority thread pool
// ----------------------------------------------------------------------------

void chaser_validate::stopping(const code& ec) NOEXCEPT
{
    // Stop threadpool keep-alive, all work must self-terminate to affect join.
    threadpool_.stop();
    chaser::stopping(ec);
}

void chaser_validate::stop() NOEXCEPT
{
    if (!threadpool_.join())
    {
        BC_ASSERT_MSG(false, "failed to join threadpool");
        std::abort();
    }
}

network::asio::strand& chaser_validate::strand() NOEXCEPT
{
    return independent_strand_;
}

bool chaser_validate::stranded() const NOEXCEPT
{
    return independent_strand_.running_in_this_thread();
}

BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
