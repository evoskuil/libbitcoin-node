/**
 * Copyright (c) 2011-2026 libbitcoin developers
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

#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

using namespace system;
using namespace database;
using namespace std::chrono;

// Non-coinbase txs of the block that cannot be validated from the pool.
static size_t unpooled_count(const query& query, const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    size_t count{};
    const auto txs = query.to_transactions(link);
    if (txs.empty())
        return count;

    for (auto tx = std::next(txs.cbegin()); tx != txs.cend(); ++tx)
    {
        pooled_tx out{};
        out.prevouts.resize(query.input_count(*tx));
        if (query.get_pooled(out, *tx, context::from(ctx)))
            ++count;
    }

    return count;
}

// Parallel execution path (concurrent by block).
// ----------------------------------------------------------------------------

void chaser_validate::validate_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    if (closed())
        return;

    code ec{};
    chain::context ctx{};
    bool batched{}, capturing{}, pooled{};
    auto& query = archive();
    const auto start = network::logger::now();
    const auto current = !bypass && is_current_header(link);

    if (!query.get_context(ctx, link))
    {
        ec = error::validate3;
    }
    else if (current && ((ec = validate_pooled(pooled, link, ctx))))
    {
        if (!node::error::error_category::contains(ec) &&
            !query.set_block_unconfirmable(link))
            ec = error::validate12;
    }
    else if (pooled)
    {
        ec = complete_pooled(link);
    }
    else if (bypass)
    {
        const auto silent = query.is_silent(link, ctx.height);
        ec = (filter_ || silent) ? complete_view(link, filter_, silent,
            silent && is_silent_capturing(link), true) : complete_bound(link);
    }
    else
    {
        auto block = query.get_block_view(link, node_witness_);

        if (!block.is_valid())
        {
            ec = error::validate2;
        }
        else if ((ec = populate(block, link, ctx)))
        {
            if (!database::error::error_category::contains(ec) &&
                !query.set_block_unconfirmable(link))
                ec = error::validate4;
        }
        else if ((ec = validate(batched, capturing, block, link, ctx)))
        {
            if (!query.set_block_unconfirmable(link))
                ec = error::validate5;
        }
    }

    if (!ec && current)
    {
        const auto elapsed = network::logger::now() - start;
        const auto usecs = duration_cast<microseconds>(elapsed).count();
        fire(events::validate_usecs, usecs);
        if (pooled)
        {
            LOGN("Validated pooled block [" << ctx.height << "] of ("
                << query.get_tx_count(link) << ") txs in (" << usecs
                << ") usecs.");
        }
        else
        {
            LOGN("Validated full block [" << ctx.height << "] of ("
                << query.get_tx_count(link) << ") txs with ("
                << unpooled_count(query, link, ctx) << ") unpooled in ("
                << usecs << ") usecs.");
        }
    }

    --validate_backlog_;
    complete_block(ec, link, ctx.height, bypass, batched, capturing);
}

// helpers
// ----------------------------------------------------------------------------

// Internal maturity and time locks are verified by internal population, the
// only necessary confirmation checks for internal spends. Every found spend
// is bound, whether or not the block is valid.
code chaser_validate::populate(chain::view::block& block,
    const header_link& link, const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();
    const auto internal = block.populate(ctx);
    const auto populated = query.populate_with_metadata(link, block);
    if (populated && database::error::error_category::contains(populated))
        return populated;

    const auto bound = query.bind(link, block);
    if (bound && bound != system::error::missing_previous_output)
        return bound;

    return internal ? internal : populated;
}

// Scan a validated block whose silent bank was dropped by restore.
void chaser_validate::scan_block(const header_link& link,
    bool bypass) NOEXCEPT
{
    if (closed())
        return;

    size_t height{};
    const auto ec = archive().get_height(height, link) ?
        complete_view(link, false, true, false, false) : error::validate3;

    --validate_backlog_;
    complete_block(ec, link, height, bypass);
}

// A bypassed block is read only to bind its inputs and for its optional
// indexes, so it is not materialized. A filter requires all prevouts,
// otherwise only those of the silent payment eligible txs are populated.
code chaser_validate::validate_view(const header_link& link, bool filter,
    bool silent, bool bank, bool bind) NOEXCEPT
{
    auto& query = archive();
    auto block = query.get_block_view(link, node_witness_ && silent);

    if (!block.is_valid())
        return error::validate2;

    if (!filter && !query.select_silent(link, block))
        return database::error::integrity;

    if (const auto ec = block.populate(chain::context{}))
        return ec;

    const auto populated = query.populate_with_metadata(link, block);
    if (populated && database::error::error_category::contains(populated))
        return populated;

    if (bind)
    {
        const auto bound = query.bind(link, block);
        if (bound && bound != system::error::missing_previous_output)
            return bound;
    }

    if (populated)
        return populated;

    if (filter && !query.set_filter_body(link, block))
        return error::validate8;

    // Banked records are set when the bank drains.
    if (silent && (bank ? !commit_silent_batch(link, block) :
        !query.set_silent(link, block)))
        return error::validate9;

    return error::success;
}

// A bypassed block without optional indexes is bound from the store alone
// (its inputs may be pruned).
code chaser_validate::complete_bound(const header_link& link) NOEXCEPT
{
    auto& query = archive();
    const auto ec = query.bind(link);
    if (ec == system::error::missing_previous_output)
        return query.set_block_unconfirmable(link) ? ec : error::validate4;

    if (ec)
        return ec;

    return query.set_block_valid(link) ? error::success : error::validate10;
}

code chaser_validate::complete_view(const header_link& link, bool filter,
    bool silent, bool bank, bool bind) NOEXCEPT
{
    auto& query = archive();
    const auto ec = validate_view(link, filter, silent, bank, bind);
    if (ec == system::error::missing_previous_output)
        return query.set_block_unconfirmable(link) ? ec : error::validate4;

    if (ec)
        return ec;

    // Valid must be set after set_filter_body.
    return query.set_block_valid(link) ? error::success : error::validate10;
}

// A block with all txs pooled under a sufficient context requires only block
// checks, performed by the store. Insufficiency implies full validation.
code chaser_validate::validate_pooled(bool& pooled, const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    const auto ec = archive().validate_pooled(link, ctx, subsidy_interval_,
        initial_subsidy_);

    pooled = !ec;
    if (!ec || ec == database::error::unvalidated)
        return error::success;

    // Store codes are faults, consensus codes imply block invalidity.
    return database::error::error_category::contains(ec) ?
        error::validate11 : ec;
}

// A pooled block is valid and SP indexed, so produce only for client filters.
code chaser_validate::complete_pooled(const header_link& link) NOEXCEPT
{
    auto& query = archive();
    if (filter_)
    {
        const auto ec = validate_view(link, true, false, false, false);
        if (ec == system::error::missing_previous_output)
            return error::validate11;

        if (ec)
            return ec;
    }

    // Pooled txs are scanned, and valid must be set after set_filter_body.
    if (!query.set_block_scanned(link))
        return error::validate9;

    return query.set_block_valid(link) ? error::success : error::validate10;
}

code chaser_validate::validate(bool& batched, bool& capturing,
    const chain::view::block& block, const header_link& link,
    const chain::context& ctx) NOEXCEPT
{
    auto& query = archive();

    code ec{};
    if (((ec = block.check())) || ((ec = block.check(ctx, signet_challenge_))))
        return ec;

    if ((ec = block.accept(ctx, subsidy_interval_, initial_subsidy_)))
        return ec;

    // Initialize signature capture (appends to this thread's accumulators).
    const auto capture = get_capture(link);
    capturing = capture.enabled;

    ec = block.connect(ctx, capture);

    // At least one signature batch was attempted (batch completion).
    batched = capture.batched;

    // Commit (or discard) the captured signatures, marking the block prevalid
    // contingent on batch verification. The commit epoch is mutually
    // exclusive with batch verification (turnstile).
    if (capturing)
    {
        if (!ec && batched)
            ec = commit_capture(batched, link);
        else
            clear_capture();
    }

    if (ec)
        return ec;

    if (!query.set_filter_body(link, block))
        return error::validate8;

    // Banked records are set when the bank drains.
    if (query.is_silent(link, ctx.height) && (is_silent_capturing(link) ?
        !commit_silent_batch(link, block) : !query.set_silent(link, block)))
        return error::validate9;

    // Defer block state change when batched.
    // Valid must be set after set_filter_body.
    if (!batched && !query.set_block_valid(link))
        return error::validate10;

    return error::success;
}

} // namespace node
} // namespace libbitcoin
