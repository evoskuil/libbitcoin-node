/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-node.
 *
 * libbitcoin-node is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NODE_FULL_NODE_HPP
#define LIBBITCOIN_NODE_FULL_NODE_HPP

#include <cstdint>
#include <memory>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/configuration.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/node_interface.hpp>
#include <bitcoin/node/sessions/session_block_sync.hpp>
#include <bitcoin/node/sessions/session_header_sync.hpp>
#include <bitcoin/node/utility/header_queue.hpp>

namespace libbitcoin {
namespace node {

/// A full node on the Bitcoin P2P network.
class BCN_API full_node
  : public network::p2p_network, public node_interface
{
public:
    typedef std::shared_ptr<full_node> ptr;

    /// Construct the full node.
    full_node(const configuration& configuration);

    /// Ensure all threads are coalesced.
    virtual ~full_node();

    // Start/Run sequences.
    // ------------------------------------------------------------------------

    /// Invoke startup and seeding sequence, call from constructing thread.
    void start(result_handler handler) override;

    /// Synchronize the blockchain and then begin long running sessions,
    /// call from start result handler. Call base method to skip sync.
    void run(result_handler handler) override;

    // Shutdown.
    // ------------------------------------------------------------------------

    /// Idempotent call to signal work stop, start may be reinvoked after.
    /// Returns the result of file save operation.
    bool stop() override;

    /// Blocking call to coalesce all work and then terminate all threads.
    /// Call from thread that constructed this class, or don't call at all.
    /// This calls stop, and start may be reinvoked after calling this.
    bool close() override;

    // Properties [node_interface].
    // ------------------------------------------------------------------------

    /// Node configuration settings.
    virtual const settings& node_settings() const;

    /// Return the current top block hash.
    virtual hash_digest top_hash() const;

    /// Blockchain query interface.
    virtual blockchain::full_chain& chain();

    // Subscriptions [node_interface].
    // ------------------------------------------------------------------------

    /// Subscribe to blockchain reorganization and stop events.
    virtual void subscribe_blockchain(reorganize_handler handler);

    /// Subscribe to transaction pool acceptance and stop events.
    virtual void subscribe_transaction(transaction_handler handler);

protected:
    /// Attach a node::session to the network, caller must start the session.
    template <class Session, typename... Args>
    typename Session::ptr attach(Args&&... args)
    {
        return std::make_shared<Session>(*this, std::forward<Args>(args)...);
    }

    /// Override to attach specialized p2p sessions.
    ////network::session_seed::ptr attach_seed_session() override;
    network::session_manual::ptr attach_manual_session() override;
    network::session_inbound::ptr attach_inbound_session() override;
    network::session_outbound::ptr attach_outbound_session() override;

    /// Override to attach specialized node sessions.
    virtual session_header_sync::ptr attach_header_sync_session();
    virtual session_block_sync::ptr attach_block_sync_session();

private:
    typedef message::block_message::ptr_list block_ptr_list;

    bool handle_reorganized(const code& ec, size_t fork_height,
        const block_const_ptr_list& incoming,
        const block_const_ptr_list& outgoing);

    void handle_headers_synchronized(const code& ec, result_handler handler);
    void handle_network_stopped(const code& ec, result_handler handler);

    void handle_started(const code& ec, result_handler handler);
    void handle_running(const code& ec, result_handler handler);

    // These are thread safe.
    bc::atomic<hash_digest> top_hash_;
    header_queue sync_hashes_;
    blockchain::block_chain blockchain_;
    const uint32_t protocol_maximum_;
    const settings& settings_;
};

} // namespace node
} //namespace libbitcoin

#endif
