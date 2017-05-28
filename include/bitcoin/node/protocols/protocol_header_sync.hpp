/////**
//// * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
//// *
//// * This file is part of libbitcoin.
//// *
//// * This program is free software: you can redistribute it and/or modify
//// * it under the terms of the GNU Affero General Public License as published by
//// * the Free Software Foundation, either version 3 of the License, or
//// * (at your option) any later version.
//// *
//// * This program is distributed in the hope that it will be useful,
//// * but WITHOUT ANY WARRANTY; without even the implied warranty of
//// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// * GNU Affero General Public License for more details.
//// *
//// * You should have received a copy of the GNU Affero General Public License
//// * along with this program.  If not, see <http://www.gnu.org/licenses/>.
//// */
////#ifndef LIBBITCOIN_NODE_PROTOCOL_HEADER_SYNC_HPP
////#define LIBBITCOIN_NODE_PROTOCOL_HEADER_SYNC_HPP
////
////#include <cstddef>
////#include <cstdint>
////#include <memory>
////#include <vector>
////#include <bitcoin/network.hpp>
////#include <bitcoin/node/configuration.hpp>
////#include <bitcoin/node/define.hpp>
////#include <bitcoin/node/utility/header_list.hpp>
////
////namespace libbitcoin {
////namespace node {
////
////class full_node;
////
/////// Headers sync protocol, thread safe.
////class BCN_API protocol_header_sync
////  : public network::protocol_timer, public track<protocol_header_sync>
////{
////public:
////    typedef std::shared_ptr<protocol_header_sync> ptr;
////
////    /// Construct a header sync protocol instance.
////    protocol_header_sync(full_node& network, network::channel::ptr channel,
////        header_list::ptr headers, uint32_t minimum_rate);
////
////    /// Start the protocol.
////    virtual void start(event_handler handler);
////
////private:
////    void send_get_headers(event_handler complete);
////    void handle_event(const code& ec, event_handler complete);
////    void headers_complete(const code& ec, event_handler handler);
////    bool handle_receive_headers(const code& ec, headers_const_ptr message,
////        event_handler complete);
////
////    // Thread safe and guarded by sequential header sync.
////    header_list::ptr headers_;
////
////    // This is guarded by protocol_timer/deadline contract (exactly one call).
////    size_t current_second_;
////
////    const uint32_t minimum_rate_;
////    const size_t start_size_;
////};
////
////} // namespace node
////} // namespace libbitcoin
////
////#endif
