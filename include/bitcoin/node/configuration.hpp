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
#ifndef LIBBITCOIN_NODE_CONFIGURATION_HPP
#define LIBBITCOIN_NODE_CONFIGURATION_HPP

#include <bitcoin/database.hpp>
#include <bitcoin/network.hpp>
#include <bitcoin/node/define.hpp>
#include <bitcoin/node/settings.hpp>

namespace libbitcoin {
namespace node {

// Not localizable.
#define BN_HELP_VARIABLE "help"
#define BN_HARDWARE_VARIABLE "hardware"
#define BN_SETTINGS_VARIABLE "settings"
#define BN_VERSION_VARIABLE "version"
#define BN_NEWSTORE_VARIABLE "newstore"
#define BN_BACKUP_VARIABLE "backup"
#define BN_RESTORE_VARIABLE "restore"

#define BN_FLAGS_VARIABLE "flags"
#define BN_SLABS_VARIABLE "slabs"
#define BN_BUCKETS_VARIABLE "buckets"
#define BN_COLLISIONS_VARIABLE "collisions"
#define BN_INFORMATION_VARIABLE "information"

#define BN_READ_VARIABLE "test"
#define BN_WRITE_VARIABLE "write"

// This must be lower case but the env var part can be any case.
#define BN_CONFIG_VARIABLE "config"

// This must match the case of the env var.
#define BN_ENVIRONMENT_VARIABLE_PREFIX "BN_"

/// Full node configuration, thread safe.
class BCN_API configuration
{
public:
    DEFAULT_COPY_MOVE_DESTRUCT(configuration);

    configuration(system::chain::selection context) NOEXCEPT;

    /// Environment.
    std::filesystem::path file;

    /// Information.
    bool help{};
    bool hardware{};
    bool settings{};
    bool version{};

    /// Actions.
    bool newstore{};
    bool backup{};
    bool restore{};

    /// Chain scans.
    bool flags{};
    bool information{};
    bool slabs{};
    bool buckets{};
    bool collisions{};

    /// Ad-hoc Testing.
    bool test{};
    bool write{};

    /// Settings.
    log::settings log;
    node::settings node;
    database::settings database;
    network::settings network;
    system::settings bitcoin;
};

} // namespace node
} // namespace libbitcoin

#endif
