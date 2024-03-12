/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_NODE_CHASERS_CHASER_BLOCK_HPP
#define LIBBITCOIN_NODE_CHASERS_CHASER_BLOCK_HPP

#include <unordered_map>
#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/chasers/chaser.hpp>

namespace libbitcoin {
namespace node {

class full_node;

/// Chase down stronger block branches for the confirmed chain.
/// Weak branches are retained in a hash table if not store populated.
/// Strong branches reorganize the candidate chain and fire the 'connect' event.
class BCN_API chaser_block
  : public chaser
{
public:
    DELETE_COPY_MOVE_DESTRUCT(chaser_block);

    chaser_block(full_node& node) NOEXCEPT;

    /// Initialize chaser state.
    virtual code start() NOEXCEPT;

    /// Validate and organize next block in sequence relative to caller peer.
    virtual void organize(const system::chain::block::cptr& block_ptr,
        organize_handler&& handler) NOEXCEPT;

protected:
    virtual void handle_event(const code& ec, chase event_, link value) NOEXCEPT;
    virtual void do_disorganize(header_t height) NOEXCEPT;
    virtual void do_organize(const system::chain::block::cptr& block,
        const organize_handler& handler) NOEXCEPT;

private:
    struct block_state
    {
        system::chain::block::cptr block;
        system::chain::chain_state::ptr state;
    };
    using block_tree = std::unordered_map<system::hash_digest, block_state>;
    using header_links = std::vector<database::header_link>;

    system::chain::chain_state::ptr get_chain_state(
        const system::hash_digest& hash) const NOEXCEPT;
    bool get_branch_work(uint256_t& work, size_t& point,
        system::hashes& tree_branch, header_links& store_branch,
        const system::chain::header& header) const NOEXCEPT;
    bool get_is_strong(bool& strong, const uint256_t& work,
        size_t point) const NOEXCEPT;
    void cache(const system::chain::block::cptr& block,
        const system::chain::chain_state::ptr& state) NOEXCEPT;
    database::header_link push_block(
        const system::chain::block::cptr& block,
        const system::chain::context& context) const NOEXCEPT;
    bool push_block(const system::hash_digest& key) NOEXCEPT;
    void populate(const system::chain::block& block) const NOEXCEPT;
    void set_prevout(const system::chain::input& input) const NOEXCEPT;

    // This is thread safe.
    const system::chain::checkpoints& checkpoints_;

    // This is protected by strand.
    system::chain::chain_state::ptr state_{};
    block_tree tree_{};
};

} // namespace node
} // namespace libbitcoin

#endif