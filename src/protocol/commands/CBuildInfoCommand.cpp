/*-------------------------------------------------------------------------
 *
 * CBuildInfoCommand.cpp
 *      BuildInfo command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CBuildInfoCommand.hpp"
#include <chrono>

namespace FauxDB
{


CBuildInfoCommand::CBuildInfoCommand()
{
    /* Constructor */
}


string
CBuildInfoCommand::getCommandName() const
{
    return "buildInfo";
}


vector<uint8_t>
CBuildInfoCommand::execute(const CommandContext& context)
{
    /* BuildInfo doesn't require database connection */
    return executeWithoutDatabase(context);
}


bool
CBuildInfoCommand::requiresDatabase() const
{
    return false; /* BuildInfo should always work */
}


vector<uint8_t>
CBuildInfoCommand::executeWithDatabase(const CommandContext& context)
{
    /* Same as without database for buildInfo */
    return executeWithoutDatabase(context);
}


vector<uint8_t>
CBuildInfoCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Comprehensive build information response */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();
    
    /* Version information */
    bson.addString("version", "7.0.0-fauxdb");
    bson.addString("gitVersion", getGitVersion());
    
    /* Build environment */
    CBsonType buildEnv = createBuildEnvironment();
    bson.addString("allocator", "tcmalloc");
    bson.addString("buildEnvironment", "darwin");
    
    /* Version array */
    bson.beginArray("versionArray");
    bson.addArrayInt32(7);
    bson.addArrayInt32(0);
    bson.addArrayInt32(0);
    bson.addArrayInt32(0);
    bson.endArray();
    
    /* JavaScript engine */
    bson.addString("javascriptEngine", "mozjs");
    
    /* Architecture and platform */
    bson.addString("sysInfo", "deprecated");
    bson.addInt32("bits", 64);
    bson.addBool("debug", false);
    bson.addInt32("maxBsonObjectSize", 16777216);
    
    /* Storage engines */
    bson.beginArray("storageEngines");
    bson.addArrayString("devnull");
    bson.addArrayString("ephemeralForTest");
    bson.addArrayString("wiredTiger");
    bson.addArrayString("postgresql"); /* Our custom storage engine */
    bson.endArray();
    
    /* Build information */
    bson.addString("buildDate", getBuildDate());
    bson.addString("compiler", getCompilerVersion());
    
    /* Target platform */
    bson.addString("targetMinOS", "macOS 10.14");
    
    /* Modules */
    CBsonType modules = createModulesInfo();
    bson.beginArray("modules");
    bson.addArrayString("enterprise");
    bson.endArray();
    
    /* OpenSSL information */
    CBsonType openssl;
    openssl.initialize();
    openssl.beginDocument();
    openssl.addString("running", "OpenSSL 3.0.0 7 sep 2021");
    openssl.addString("compiled", "OpenSSL 3.0.0 7 sep 2021");
    openssl.endDocument();
    bson.addDocument("openssl", openssl);
    
    /* Feature compatibility */
    CBsonType featureCompatibility = createFeatureCompatibility();
    
    /* Build flags */
    bson.addString("buildFlags", "-O3 -Wall -Wextra -std=c++23");
    
    /* Success indicator */
    bson.addDouble("ok", 1.0);
    
    bson.endDocument();
    return bson.getDocument();
}


CBsonType
CBuildInfoCommand::createVersionInfo()
{
    CBsonType version;
    version.initialize();
    version.beginDocument();
    
    version.addString("version", "7.0.0-fauxdb");
    version.addString("gitVersion", getGitVersion());
    
    version.beginArray("versionArray");
    version.addArrayInt32(7);
    version.addArrayInt32(0);
    version.addArrayInt32(0);
    version.addArrayInt32(0);
    version.endArray();
    
    version.endDocument();
    return version;
}


CBsonType
CBuildInfoCommand::createBuildEnvironment()
{
    CBsonType env;
    env.initialize();
    env.beginDocument();
    
    env.addString("distmod", "");
    env.addString("distarch", "x86_64");
    env.addString("cc", "clang");
    env.addString("ccflags", "-Wall -Wextra -O3");
    env.addString("cxx", "clang++");
    env.addString("cxxflags", "-std=c++23 -Wall -Wextra -O3");
    env.addString("linkflags", "-L/usr/local/lib");
    env.addString("target_arch", "x86_64");
    env.addString("target_os", "macOS");
    
    env.endDocument();
    return env;
}


CBsonType
CBuildInfoCommand::createFeatureCompatibility()
{
    CBsonType compat;
    compat.initialize();
    compat.beginDocument();
    
    compat.addString("version", "7.0");
    
    compat.endDocument();
    return compat;
}


CBsonType
CBuildInfoCommand::createModulesInfo()
{
    CBsonType modules;
    modules.initialize();
    modules.beginDocument();
    
    modules.addString("enterprise", "enabled");
    modules.addString("postgresql", "enabled");
    
    modules.endDocument();
    return modules;
}


string
CBuildInfoCommand::getBuildDate()
{
    /* Get current date as build date */
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", std::gmtime(&time_t));
    
    return string(buffer);
}


string
CBuildInfoCommand::getGitVersion()
{
    /* Mock git version */
    return "fauxdb-v1.0.0-1234567890abcdef";
}


string
CBuildInfoCommand::getCompilerVersion()
{
    /* Get compiler version information */
    #ifdef __clang__
        return "clang version " __clang_version__;
    #elif defined(__GNUC__)
        return "gcc " __VERSION__;
    #else
        return "unknown compiler";
    #endif
}

} // namespace FauxDB
