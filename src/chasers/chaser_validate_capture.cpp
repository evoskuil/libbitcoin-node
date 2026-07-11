/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#include <bitcoin/node/define.hpp>

namespace libbitcoin {
namespace node {

#define CLASS chaser_validate

using namespace system;
using namespace system::chain;
using namespace database;
using namespace std::placeholders;

// Shared pointers required for lifetime in handler parameters.
BC_PUSH_WARNING(NO_VALUE_OR_CONST_REF_SHARED_PTR)
BC_PUSH_WARNING(SMART_PTR_NOT_NEEDED)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

// Capture handlers.
// ----------------------------------------------------------------------------
// private

void chaser_validate::do_log(const script& ) NOEXCEPT
{
    // Enable for a game of whack-a-mole.
    ////LOGA("Sigop @ " << ctx.height << " -> "
    ////    << missed.to_string(flags::all_rules));
}

void chaser_validate::do_fire(missed miss, size_t count) NOEXCEPT
{
    switch (miss)
    {
        case missed::ecdsa:
            counters_.missed_ecdsa_ += count;
            break;
        case missed::multisig:
            counters_.missed_multisig_ += count;
            break;
        case missed::schnorr:
            counters_.missed_schnorr_ += count;
            break;
        default:;
    }
}

bool chaser_validate::do_ecdsa(const hash_digest& digest,
    const ec_compressed& point, const ec_signature& sign,
    const header_link& link, const atomic_counter_ptr& sequence) NOEXCEPT
{
    ++counters_.ecdsa_;
    const auto id = (*sequence)++;
    if (is_limited<uint16_t>(id)) return false;
    const auto group = narrow_cast<uint16_t>(id);
    const auto set = archive().set_signature(digest, point, sign, group, link);
    if (!set) fault(error::batch5);
    return set;
}

bool chaser_validate::do_schnorr(const hash_digest& digest,
    const ec_xonly& point, const ec_signature& sign,
    const header_link& link) NOEXCEPT
{
    ++counters_.schnorr_;
    const auto set = archive().set_signature(digest, point, sign, link);
    if (!set) fault(error::batch6);
    return set;
}

bool chaser_validate::do_multisig(const hash_digest& digest,
    const std::span<const system::ec_compressed>& points,
    const std::span<const system::ec_signature>& signs,
    const header_link& link, const atomic_counter_ptr& sequence) NOEXCEPT
{
    counters_.multisig_ += points.size();
    const auto id = (*sequence)++;
    if (is_limited<uint16_t>(id)) return false;
    const auto group = narrow_cast<uint16_t>(id);
    const auto set = archive().set_signatures(digest, points, signs, group, link);
    if (!set) fault(error::batch7);
    return set;
}

bool chaser_validate::do_threshold(const hash_digest& digest,
    const ec_xonly& point, const ec_signature& sign,
    const schnorr_link_ptr& fk_ptr, const header_link& link) NOEXCEPT
{
    auto set = archive().set_signature((*fk_ptr)++, digest, point, sign, link);
    if (!set) fault(error::batch8);
    return set;
}

chaser_validate::cursor chaser_validate::open_threshold(size_t rows,
    const header_link& link) NOEXCEPT
{
    auto first = archive().allocate_signatures(rows);
    const auto fk = emplace_shared<schnorr_link>(first);
    if (fk->is_terminal())
        return {};

    counters_.threshold_ += rows;
    return
    {
        .put = BIND_THIS(do_threshold, _1, _2, _3, fk, link),
        .rows = rows
    };
}

// Capture helpers.
// ----------------------------------------------------------------------------
// private

signatures chaser_validate::get_capture(const header_link& link) NOEXCEPT
{
    if (!batch_enabled_ || is_current_header(link))
        return { false };

    const auto sequence = to_shared<atomic_counter>();
    return signatures
    {
        .enabled = true,
        .log = BIND_THIS(do_log, _1),
        .fire = BIND_THIS(do_fire, _1, _2),
        .ecdsa = BIND_THIS(do_ecdsa, _1, _2, _3, link, sequence),
        .schnorr = BIND_THIS(do_schnorr, _1, _2, _3, link),
        .multisig = BIND_THIS(do_multisig, _1, _2, _3, link, sequence),
        .threshold = BIND_THIS(open_threshold, _1, link)
    };
}

std::string chaser_validate::log_ratio(const std::string& name,
    size_t numerator, size_t denominator) const NOEXCEPT
{
    if (is_zero(denominator))
        return name;

    const auto ratio = (100.0 * numerator) / denominator;
    return (boost_format("%1% (%2% / %3%) = %4$.4f%%") %
        name % numerator % denominator % ratio).str();
}

void chaser_validate::log_captures() const NOEXCEPT
{
    LOGN(log_ratio("Capture ecdsa.... ", counters_.ecdsa_,
        counters_.ecdsa_     + counters_.missed_ecdsa_));
    LOGN(log_ratio("Capture multisig. ", counters_.multisig_,
        counters_.multisig_  + counters_.missed_multisig_));
    LOGN(log_ratio("Capture schnorr.. ", counters_.schnorr_,
        counters_.schnorr_   + counters_.missed_schnorr_));
    LOGN(log_ratio("Capture threshold ", counters_.threshold_,
        counters_.threshold_ + zero));
}

BC_POP_WARNING()
BC_POP_WARNING()
BC_POP_WARNING()

} // namespace node
} // namespace libbitcoin
