/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
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
#include "executor.hpp"

#include <csignal>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <boost/core/null_deleter.hpp>
#include <boost/format.hpp>
#include <bitcoin/node.hpp>

namespace libbitcoin {
namespace node {

using boost::format;
using namespace bc::system;
using namespace bc::system::config;
using namespace bc::database;
using namespace bc::network;
using namespace std::placeholders;

static const auto application_name = "bn";
static constexpr int initialize_stop = 0;

std::promise<code> executor::stopping_;

executor::executor(parser& metadata, std::istream&,
    std::ostream& output, std::ostream& error)
  : metadata_(metadata), output_(output), error_(error)
{
    handle_stop(initialize_stop);
}

// Command line options.
// ----------------------------------------------------------------------------
// Emit directly to standard output (not the log).

void executor::do_help()
{
    const auto options = metadata_.load_options();
    printer help(options, application_name, BN_INFORMATION_MESSAGE);
    help.initialize();
    help.commandline(output_);
}

void executor::do_settings()
{
    const auto settings = metadata_.load_settings();
    printer print(settings, application_name, BN_SETTINGS_MESSAGE);
    print.initialize();
    print.settings(output_);
}

void executor::do_version()
{
    output_ << format(BN_VERSION_MESSAGE) %
        LIBBITCOIN_NODE_VERSION %
        LIBBITCOIN_BLOCKCHAIN_VERSION %
        LIBBITCOIN_SYSTEM_VERSION << std::endl;
}

// Emit to the log.
bool executor::do_initchain()
{
    initialize_output();
    const auto& directory = metadata_.configured.database.dir;

    if (!file::create_directory(directory))
    {
        LOG_ERROR(LOG_NODE) << format(BN_INITCHAIN_EXISTS) % directory;
        return false;
    }

    LOG_INFO(LOG_NODE) << format(BN_INITIALIZING_CHAIN) % directory;

    ////const auto& settings_chain = metadata_.configured.chain;
    ////const auto& settings_database = metadata_.configured.database;
    ////const auto& settings_system = metadata_.configured.bitcoin;

    system::code code{};
    ////system::code code = bc::blockchain::block_chain_initializer(
    ////    settings_chain, settings_database, settings_system).create(
    ////        settings_system.genesis_block);

    if (code)
    {
        LOG_ERROR(LOG_NODE) <<
            format(BN_INITCHAIN_DATABASE_CREATE_FAILURE) % code.message();
        return false;
    }

    LOG_INFO(LOG_NODE) << BN_INITCHAIN_COMPLETE;
    return true;
}

// Menu selection.
// ----------------------------------------------------------------------------

bool executor::menu()
{
    const auto& config = metadata_.configured;

    if (config.help)
    {
        do_help();
        return true;
    }

    if (config.settings)
    {
        do_settings();
        return true;
    }

    if (config.version)
    {
        do_version();
        return true;
    }

    if (config.initchain)
    {
        return do_initchain();
    }

    // There are no command line arguments, just run the node.
    return run();
}

// Run.
// ----------------------------------------------------------------------------

bool executor::run()
{
    initialize_output();

    LOG_INFO(LOG_NODE) << BN_NODE_INTERRUPT;
    LOG_INFO(LOG_NODE) << BN_NODE_STARTING;

    if (!verify_directory())
        return false;

    node_ = std::make_shared<full_node>(metadata_.configured);
    node_->start(std::bind(&executor::handle_started, this, _1));
    stopping_.get_future().wait();

    LOG_INFO(LOG_NODE) << BN_NODE_STOPPING;

    // Block on work stoppage, must be called from main thread.
    node_->close();
    return true;
}

void executor::handle_started(const code& ec)
{
    if (ec)
    {
        LOG_ERROR(LOG_NODE) << format(BN_NODE_START_FAIL) % ec.message();
        stop(ec);
        return;
    }

    LOG_INFO(LOG_NODE) << BN_NODE_SEEDED;

    node_->subscribe_close(
        std::bind(&executor::handle_handler, this, _1),
        std::bind(&executor::handle_stopped, this, _1));
}

void executor::handle_handler(const code& ec)
{
    if (ec)
    {
        LOG_INFO(LOG_NODE) << format(BN_NODE_START_FAIL) % ec.message();
        stop(ec);
        return;
    }

    node_->run(std::bind(&executor::handle_running, this, _1));
}

void executor::handle_running(const code& ec)
{
    if (ec)
    {
        LOG_INFO(LOG_NODE) << format(BN_NODE_START_FAIL) % ec.message();
        stop(ec);
        return;
    }

    LOG_INFO(LOG_NODE) << BN_NODE_STARTED;
}

void executor::handle_stopped(const code& ec)
{
    stop(ec);
}

// Stop signal.
// ----------------------------------------------------------------------------

void executor::handle_stop(int code)
{
    std::signal(SIGINT, handle_stop);
    std::signal(SIGTERM, handle_stop);

    if (code == initialize_stop)
        return;

    LOG_INFO(LOG_NODE) << format(BN_NODE_SIGNALED) % code;
    stop(system::error::success);
}

// Manage the race between console stop and server stop.
void executor::stop(const code& ec)
{
    static std::once_flag stop_mutex;
    std::call_once(stop_mutex, [&](){ stopping_.set_value(ec); });
}

// Utilities.
// ----------------------------------------------------------------------------

void executor::initialize_output()
{
    const auto header = format(BN_LOG_HEADER) % local_time();
    LOG_DEBUG(LOG_NODE)   << header;
    LOG_INFO(LOG_NODE)    << header;
    LOG_WARNING(LOG_NODE) << header;
    LOG_ERROR(LOG_NODE)   << header;
    LOG_FATAL(LOG_NODE)   << header;

    const auto& file = metadata_.configured.file;

    if (file.empty())
        LOG_INFO(LOG_NODE) << BN_USING_DEFAULT_CONFIG;
    else
        LOG_INFO(LOG_NODE) << format(BN_USING_CONFIG_FILE) % file;
}

// Use missing directory as a sentinel indicating lack of initialization.
bool executor::verify_directory()
{
    const auto& directory = metadata_.configured.database.dir;

    if (exists(directory))
        return true;

    LOG_ERROR(LOG_NODE) << format(BN_UNINITIALIZED_CHAIN) % directory;
    return false;
}

} // namespace node
} // namespace libbitcoin
