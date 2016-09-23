/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/node/protocols/protocol_block_out.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <functional>
#include <string>
#include <bitcoin/blockchain.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/p2p_node.hpp>

namespace libbitcoin {
namespace node {

#define NAME "block"
#define CLASS protocol_block_out

using namespace bc::blockchain;
using namespace bc::message;
using namespace bc::network;
using namespace std::placeholders;

// Protocol limit.
static constexpr auto headers_cap = 2000u;
static constexpr auto inventory_cap = 500u;

protocol_block_out::protocol_block_out(p2p_node& network, channel::ptr channel,
    full_chain& blockchain)
  : protocol<protocol_events>(network, channel, NAME),
    last_locator_top_(null_hash),
    blockchain_(blockchain),

    // TODO: move send_headers to a derived class protocol_block_out_70012.
    headers_to_peer_(negotiated_version() >= version::level::bip130),

    CONSTRUCT_TRACK(protocol_block_out)
{
}

// Start.
//-----------------------------------------------------------------------------

void protocol_block_out::start()
{
    protocol_events::start(BIND1(handle_stop, _1));

    // TODO: move send_headers to a derived class protocol_block_out_70012.
    if (headers_to_peer_)
    {
        // Send headers vs. inventory anncements if headers_to_peer_ is set.
        SUBSCRIBE2(send_headers, handle_receive_send_headers, _1, _2);
    }

    // TODO: move get_headers to a derived class protocol_block_out_31800.
    SUBSCRIBE2(get_headers, handle_receive_get_headers, _1, _2);
    SUBSCRIBE2(get_blocks, handle_receive_get_blocks, _1, _2);
    SUBSCRIBE2(get_data, handle_receive_get_data, _1, _2);

    // Subscribe to block acceptance notifications (our heartbeat).
    blockchain_.subscribe_reorganize(
        BIND4(handle_reorganized, _1, _2, _3, _4));
}

// Receive send_headers.
//-----------------------------------------------------------------------------

// TODO: move send_headers to a derived class protocol_block_out_70012.
bool protocol_block_out::handle_receive_send_headers(const code& ec,
    send_headers_const_ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting " << message->command << " from ["
            << authority() << "] " << ec.message();
        stop(ec);
        return false;
    }

    // Block annoucements will be headers messages instead of inventory.
    headers_to_peer_.store(true);

    // Do not resubscribe after handling this one-time message.
    return false;
}

// Receive get_headers sequence.
//-----------------------------------------------------------------------------

// TODO: move get_headers to a derived class protocol_block_out_31800.
bool protocol_block_out::handle_receive_get_headers(const code& ec,
    get_headers_const_ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting get_headers from [" << ec.message();
        stop(ec);
        return false;
    }

    const auto locator_size = message->start_hashes.size();

    // The locator cannot be longer than allowed by our chain length.
    // This is DoS protection, otherwise a peer could tie up our database.
    // If we are not synced to near the height of peers then this effectively
    // prevents peers from syncing from us. Ideally we should use initial block
    // download to get close before enabling this protocol.
    if (locator_size > chain::block::locator_size(top_height()) + 1)
    {
        log::debug(LOG_NODE)
            << "Invalid get_headers locator size (" << locator_size
            << ") from [" << authority() << "] ";
        stop(error::channel_stopped);
        return false;
    }

    // The peer threshold prevents a peer from creating an unnecessary backlog
    // for itself in the case where it is requesting without having processed
    // all of its existing backlog. This also reduces its load on us.
    // This could cause a problem during a reorg, where the peer regresses
    // and one of its other peers populates the chain back to this level. In
    // that case we would not respond but our peer's other peer should.
    const auto threshold = last_locator_top_.load();

    blockchain_.fetch_locator_block_headers(message, threshold, headers_cap,
        BIND2(handle_fetch_locator_headers, _1, _2));
    return true;
}

// TODO: move headers to a derived class protocol_block_out_31800.
void protocol_block_out::handle_fetch_locator_headers(const code& ec,
    headers_ptr message)
{
    if (stopped() || ec == error::service_stopped ||
        message->elements.empty())
        return;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating locator block headers for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    // Respond to get_headers with headers.
    SEND2(*message, handle_send, _1, message->command);

    // Save the locator top to limit an overlapping future request.
    last_locator_top_.store(message->elements.front().hash());
}

// Receive get_blocks sequence.
//-----------------------------------------------------------------------------

bool protocol_block_out::handle_receive_get_blocks(const code& ec,
    get_blocks_const_ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting get_blocks from [" << ec.message();
        stop(ec);
        return false;
    }

    const auto locator_size = message->start_hashes.size();

    if (locator_size > chain::block::locator_size(top_height()) + 1)
    {
        log::debug(LOG_NODE)
            << "Invalid get_blocks locator size (" << locator_size
            << ") from [" << authority() << "] ";
        stop(error::channel_stopped);
        return false;
    }

    const auto threshold = last_locator_top_.load();

    blockchain_.fetch_locator_block_hashes(message, threshold, inventory_cap,
        BIND2(handle_fetch_locator_hashes, _1, _2));
    return true;
}

void protocol_block_out::handle_fetch_locator_hashes(const code& ec,
    inventory_ptr message)
{
    if (stopped() || ec == error::service_stopped || 
        message->inventories.empty())
        return;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating locator block hashes for ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    // Respond to get_blocks with inventory.
    SEND2(*message, handle_send, _1, message->command);

    // Save the locator top to limit an overlapping future request.
    last_locator_top_.store(message->inventories.front().hash);
}

// Receive get_data sequence.
//-----------------------------------------------------------------------------

// TODO: move filtered_block to derived class protocol_block_out_70001.
bool protocol_block_out::handle_receive_get_data(const code& ec,
    get_data_const_ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NODE)
            << "Failure getting inventory from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    // Ignore non-block inventory requests in this protocol.
    for (const auto& inventory: message->inventories)
    {
        if (inventory.type == inventory::type_id::block)
            blockchain_.fetch_block(inventory.hash,
                BIND4(send_block, _1, _2, _3, inventory.hash));
        else if (inventory.type == inventory::type_id::filtered_block)
            blockchain_.fetch_merkle_block(inventory.hash,
                BIND4(send_merkle_block, _1, _2, _3, inventory.hash));
    }

    return true;
}

// TODO: move not_found to derived class protocol_block_out_70001.
void protocol_block_out::send_block(const code& ec, block_ptr message,
    uint64_t, const hash_digest& hash)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log::debug(LOG_NODE)
            << "Block requested by [" << authority() << "] not found.";

        const not_found reply{ { inventory::type_id::block, hash } };
        SEND2(reply, handle_send, _1, reply.command);
        return;
    }

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating block requested by ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    SEND2(*message, handle_send, _1, message->command);
}

// TODO: move filtered_block to derived class protocol_block_out_70001.
void protocol_block_out::send_merkle_block(const code& ec,
    merkle_block_ptr message, uint64_t, const hash_digest& hash)
{
    if (stopped() || ec == error::service_stopped)
        return;

    if (ec == error::not_found)
    {
        log::debug(LOG_NODE)
            << "Merkle block requested by [" << authority() << "] not found.";

        const not_found reply{ { inventory::type_id::filtered_block, hash } };
        SEND2(reply, handle_send, _1, reply.command);
        return;
    }

    if (ec)
    {
        log::error(LOG_NODE)
            << "Internal failure locating merkle block requested by ["
            << authority() << "] " << ec.message();
        stop(ec);
        return;
    }

    ////TODO: populate message->flags internal to merkle_block.
    SEND2(*message, handle_send, _1, message->command);
}

// Subscription.
//-----------------------------------------------------------------------------

// TODO: make sure we are announcing older blocks first here.
// We never announce or inventory an orphan, only indexed blocks.
bool protocol_block_out::handle_reorganized(const code& ec, size_t fork_height,
    const block_const_ptr_list& incoming, const block_const_ptr_list& outgoing)
{
    if (stopped() || ec == error::service_stopped)
        return false;

    if (ec)
    {
        log::error(LOG_NODE)
            << "Failure handling reorganization: " << ec.message();
        stop(ec);
        return false;
    }

    // TODO: move announce headers to a derived class protocol_block_in_70012.
    if (headers_to_peer_)
    {
        headers announcement;

        for (const auto block: incoming)
            if (block->originator() != nonce())
                announcement.elements.push_back(block->header);

        if (!announcement.elements.empty())
            SEND2(announcement, handle_send, _1, announcement.command);
        return true;
    }

    static const auto id = inventory::type_id::block;
    inventory announcement;

    for (const auto block: incoming)
        if (block->originator() != nonce())
            announcement.inventories.push_back( { id, block->header.hash() });

    if (!announcement.inventories.empty())
        SEND2(announcement, handle_send, _1, announcement.command);
    return true;
}

void protocol_block_out::handle_stop(const code&)
{
    log::debug(LOG_NETWORK)
        << "Stopped block_out protocol";
}

} // namespace node
} // namespace libbitcoin
