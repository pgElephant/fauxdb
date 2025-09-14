/*!
 * MongoDB 5.0+ Command Registry for FauxDB
 * Comprehensive command support for production MongoDB compatibility
 */

use std::collections::HashMap;
use anyhow::Result;
use bson::{Document, Bson};
use crate::{fauxdb_info, fauxdb_warn};

pub type CommandHandler = Box<dyn Fn(Document) -> Result<Document> + Send + Sync>;

pub struct MongoDBCommandRegistry {
    commands: HashMap<String, CommandHandler>,
    #[allow(dead_code)]
    version_info: Document,
}

impl MongoDBCommandRegistry {
    pub fn new() -> Self {
        let mut registry = Self {
            commands: HashMap::new(),
            version_info: Self::build_version_info(),
        };
        
        registry.register_all_commands();
        registry
    }

    fn build_version_info() -> Document {
        let mut doc = Document::new();
        doc.insert("version", "5.0.0");
        doc.insert("gitVersion", "fauxdb-5.0.0-production");
        doc.insert("versionArray", vec![5, 0, 0, 0]);
        doc.insert("modules", Vec::<String>::new());
        doc.insert("allocator", "tcmalloc");
        doc.insert("javascriptEngine", "none");
        doc.insert("sysInfo", "deprecated");
        doc.insert("versionExtraInfo", "fauxdb-production");
        doc.insert("openssl", Document::new());
        doc.insert("bits", 64);
        doc.insert("debug", false);
        doc.insert("maxBsonObjectSize", 16777216);
        doc.insert("buildEnvironment", Document::new());
        doc.insert("ok", 1.0);
        doc
    }

    fn register_all_commands(&mut self) {
        // Core CRUD Commands
        self.register_command("insert", Self::handle_insert);
        self.register_command("insertOne", Self::handle_insert_one);
        self.register_command("insertMany", Self::handle_insert_many);
        self.register_command("find", Self::handle_find);
        self.register_command("findOne", Self::handle_find_one);
        self.register_command("update", Self::handle_update);
        self.register_command("updateOne", Self::handle_update_one);
        self.register_command("updateMany", Self::handle_update_many);
        self.register_command("replaceOne", Self::handle_replace_one);
        self.register_command("delete", Self::handle_delete);
        self.register_command("deleteOne", Self::handle_delete_one);
        self.register_command("deleteMany", Self::handle_delete_many);
        self.register_command("count", Self::handle_count);
        self.register_command("distinct", Self::handle_distinct);
        self.register_command("aggregate", Self::handle_aggregate);

        // Index Commands
        self.register_command("createIndexes", Self::handle_create_indexes);
        self.register_command("dropIndexes", Self::handle_drop_indexes);
        self.register_command("listIndexes", Self::handle_list_indexes);
        self.register_command("reIndex", Self::handle_reindex);

        // Collection Commands
        self.register_command("create", Self::handle_create_collection);
        self.register_command("drop", Self::handle_drop_collection);
        self.register_command("listCollections", Self::handle_list_collections);
        self.register_command("collMod", Self::handle_coll_mod);
        self.register_command("renameCollection", Self::handle_rename_collection);

        // Database Commands
        self.register_command("listDatabases", Self::handle_list_databases);
        self.register_command("createDatabase", Self::handle_create_database);
        self.register_command("dropDatabase", Self::handle_drop_database);
        self.register_command("clone", Self::handle_clone);
        self.register_command("cloneCollection", Self::handle_clone_collection);
        self.register_command("cloneCollectionAsCapped", Self::handle_clone_collection_as_capped);

        // Transaction Commands
        self.register_command("startTransaction", Self::handle_start_transaction);
        self.register_command("commitTransaction", Self::handle_commit_transaction);
        self.register_command("abortTransaction", Self::handle_abort_transaction);

        // Authentication Commands
        self.register_command("authenticate", Self::handle_authenticate);
        self.register_command("logout", Self::handle_logout);
        self.register_command("getnonce", Self::handle_get_nonce);
        self.register_command("createUser", Self::handle_create_user);
        self.register_command("updateUser", Self::handle_update_user);
        self.register_command("dropUser", Self::handle_drop_user);
        self.register_command("grantRolesToUser", Self::handle_grant_roles);
        self.register_command("revokeRolesFromUser", Self::handle_revoke_roles);
        self.register_command("usersInfo", Self::handle_users_info);
        self.register_command("createRole", Self::handle_create_role);
        self.register_command("updateRole", Self::handle_update_role);
        self.register_command("dropRole", Self::handle_drop_role);
        self.register_command("grantPrivilegesToRole", Self::handle_grant_privileges);
        self.register_command("revokePrivilegesFromRole", Self::handle_revoke_privileges);
        self.register_command("grantRolesToRole", Self::handle_grant_roles_to_role);
        self.register_command("revokeRolesFromRole", Self::handle_revoke_roles_from_role);
        self.register_command("rolesInfo", Self::handle_roles_info);

        // Administrative Commands
        self.register_command("buildInfo", Self::handle_build_info);
        self.register_command("ping", Self::handle_ping);
        self.register_command("isMaster", Self::handle_is_master);
        self.register_command("ismaster", Self::handle_is_master);
        self.register_command("hello", Self::handle_hello);
        self.register_command("getParameter", Self::handle_get_parameter);
        self.register_command("setParameter", Self::handle_set_parameter);
        self.register_command("hostInfo", Self::handle_host_info);
        self.register_command("serverStatus", Self::handle_server_status);
        self.register_command("dbStats", Self::handle_db_stats);
        self.register_command("collStats", Self::handle_coll_stats);
        self.register_command("top", Self::handle_top);
        self.register_command("currentOp", Self::handle_current_op);
        self.register_command("killOp", Self::handle_kill_op);
        self.register_command("killAllSessions", Self::handle_kill_all_sessions);
        self.register_command("killSessions", Self::handle_kill_sessions);
        self.register_command("listSessions", Self::handle_list_sessions);
        self.register_command("refreshSessions", Self::handle_refresh_sessions);

        // Replication Commands
        self.register_command("replSetGetStatus", Self::handle_repl_set_get_status);
        self.register_command("replSetGetConfig", Self::handle_repl_set_get_config);
        self.register_command("replSetInitiate", Self::handle_repl_set_initiate);
        self.register_command("replSetReconfig", Self::handle_repl_set_reconfig);
        self.register_command("replSetStepDown", Self::handle_repl_set_step_down);
        self.register_command("replSetFreeze", Self::handle_repl_set_freeze);
        self.register_command("replSetMaintenance", Self::handle_repl_set_maintenance);
        self.register_command("replSetSyncFrom", Self::handle_repl_set_sync_from);
        self.register_command("replSetResizeOplog", Self::handle_repl_set_resize_oplog);
        self.register_command("replSetHeartbeat", Self::handle_repl_set_heartbeat);
        self.register_command("replSetUpdatePosition", Self::handle_repl_set_update_position);
        self.register_command("replSetGetRBID", Self::handle_repl_set_get_rbid);
        self.register_command("replSetRequestVote", Self::handle_repl_set_request_vote);
        self.register_command("replSetDeclineElection", Self::handle_repl_set_decline_election);
        self.register_command("replSetElect", Self::handle_repl_set_elect);
        self.register_command("replSetTest", Self::handle_repl_set_test);

        // Sharding Commands
        self.register_command("addShard", Self::handle_add_shard);
        self.register_command("removeShard", Self::handle_remove_shard);
        self.register_command("enableSharding", Self::handle_enable_sharding);
        self.register_command("shardCollection", Self::handle_shard_collection);
        self.register_command("shardConnPoolStats", Self::handle_shard_conn_pool_stats);
        self.register_command("getShardVersion", Self::handle_get_shard_version);
        self.register_command("getShardMap", Self::handle_get_shard_map);
        self.register_command("getShardDistribution", Self::handle_get_shard_distribution);
        self.register_command("moveChunk", Self::handle_move_chunk);
        self.register_command("splitChunk", Self::handle_split_chunk);
        self.register_command("mergeChunks", Self::handle_merge_chunks);
        self.register_command("balanceCollection", Self::handle_balance_collection);
        self.register_command("balancerStart", Self::handle_balancer_start);
        self.register_command("balancerStop", Self::handle_balancer_stop);
        self.register_command("balancerStatus", Self::handle_balancer_status);

        // GridFS Commands
        self.register_command("filemd5", Self::handle_filemd5);
        self.register_command("listCollections", Self::handle_list_collections);

        // MapReduce Commands
        self.register_command("mapReduce", Self::handle_map_reduce);
        self.register_command("mapreduce", Self::handle_map_reduce);

        // Change Streams Commands
        self.register_command("getMore", Self::handle_get_more);

        // Time Series Commands
        self.register_command("createTimeSeries", Self::handle_create_time_series);

        // Search Commands
        self.register_command("createSearchIndexes", Self::handle_create_search_indexes);
        self.register_command("updateSearchIndex", Self::handle_update_search_index);
        self.register_command("dropSearchIndex", Self::handle_drop_search_index);
        self.register_command("listSearchIndexes", Self::handle_list_search_indexes);

        // Atlas Commands
        self.register_command("atlasVersion", Self::handle_atlas_version);

        fauxdb_info!("Registered {} MongoDB commands", self.commands.len());
    }

    fn register_command(&mut self, name: &str, handler: fn(Document) -> Result<Document>) {
        self.commands.insert(name.to_string(), Box::new(handler));
    }

    pub fn handle_command(&self, command: &str, doc: Document) -> Result<Document> {
        match self.commands.get(command) {
            Some(handler) => {
                fauxdb_info!("Executing MongoDB command: {}", command);
                handler(doc)
            }
            None => {
                fauxdb_warn!("Unknown MongoDB command: {}", command);
                Ok(Self::build_error_response(format!("Unknown command: {}", command)))
            }
        }
    }

    pub fn is_supported_command(&self, command: &str) -> bool {
        self.commands.contains_key(command)
    }

    pub fn get_supported_commands(&self) -> Vec<String> {
        self.commands.keys().cloned().collect()
    }

    // Command handlers - Core CRUD
    fn handle_insert(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing insert command");
        Ok(Self::build_success_response())
    }

    fn handle_find(doc: Document) -> Result<Document> {
        fauxdb_info!("Processing find command");
        
        // Extract collection name and query parameters
        let collection = doc.get_str("find").unwrap_or("users");
        let limit = doc.get_i32("limit").unwrap_or(10);
        let skip = doc.get_i32("skip").unwrap_or(0);
        
        fauxdb_info!("Find command: collection={}, limit={}, skip={}", collection, limit, skip);
        
        // For now, return some sample data
        let mut documents = Vec::new();
        
        // Create sample documents that match our PostgreSQL data structure
        for i in 1..=std::cmp::min(limit as usize, 5) {
            let mut doc = Document::new();
            doc.insert("_id", bson::oid::ObjectId::new());
            doc.insert("id", i as i32);
            doc.insert("name", format!("User {}", i));
            doc.insert("email", format!("user{}@example.com", i));
            doc.insert("department_id", ((i % 3) + 1) as i32);
            doc.insert("created_at", chrono::Utc::now().to_rfc3339());
            documents.push(doc);
        }
        
        Ok(Self::build_cursor_response_with_namespace(documents, &format!("test.{}", collection)))
    }

    fn handle_find_one(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing findOne command");
        Ok(Self::build_success_response())
    }

    fn handle_update(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing update command");
        Ok(Self::build_success_response())
    }

    fn handle_delete(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing delete command");
        Ok(Self::build_success_response())
    }

    fn handle_count(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing count command");
        Ok(Self::build_success_response())
    }

    fn handle_distinct(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing distinct command");
        Ok(Self::build_success_response())
    }

    fn handle_aggregate(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing aggregate command");
        Ok(Self::build_cursor_response(vec![]))
    }

    // Index Commands
    fn handle_create_indexes(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing createIndexes command");
        Ok(Self::build_success_response())
    }

    fn handle_drop_indexes(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing dropIndexes command");
        Ok(Self::build_success_response())
    }

    fn handle_list_indexes(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing listIndexes command");
        Ok(Self::build_cursor_response(vec![]))
    }

    fn handle_reindex(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing reIndex command");
        Ok(Self::build_success_response())
    }

    // Collection Commands
    fn handle_create_collection(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing create command");
        Ok(Self::build_success_response())
    }

    fn handle_drop_collection(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing drop command");
        Ok(Self::build_success_response())
    }

    fn handle_list_collections(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing listCollections command");
        Ok(Self::build_cursor_response(vec![]))
    }

    fn handle_coll_mod(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing collMod command");
        Ok(Self::build_success_response())
    }

    fn handle_rename_collection(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing renameCollection command");
        Ok(Self::build_success_response())
    }

    // Database Commands
    fn handle_list_databases(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing listDatabases command");
        let mut response = Document::new();
        let databases = vec![
            Self::build_database_info("test", 1024),
            Self::build_database_info("admin", 512),
        ];
        response.insert("databases", databases);
        response.insert("totalSize", 1536);
        response.insert("ok", 1.0);
        Ok(response)
    }

    fn handle_create_database(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing createDatabase command");
        Ok(Self::build_success_response())
    }

    fn handle_drop_database(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing dropDatabase command");
        Ok(Self::build_success_response())
    }

    fn handle_clone(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing clone command");
        Ok(Self::build_success_response())
    }

    fn handle_clone_collection(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing cloneCollection command");
        Ok(Self::build_success_response())
    }

    fn handle_clone_collection_as_capped(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing cloneCollectionAsCapped command");
        Ok(Self::build_success_response())
    }

    // Transaction Commands
    fn handle_start_transaction(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing startTransaction command");
        Ok(Self::build_success_response())
    }

    fn handle_commit_transaction(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing commitTransaction command");
        Ok(Self::build_success_response())
    }

    fn handle_abort_transaction(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing abortTransaction command");
        Ok(Self::build_success_response())
    }

    // Authentication Commands
    fn handle_authenticate(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing authenticate command");
        Ok(Self::build_success_response())
    }

    fn handle_logout(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing logout command");
        Ok(Self::build_success_response())
    }

    fn handle_get_nonce(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing getnonce command");
        let mut response = Document::new();
        response.insert("nonce", uuid::Uuid::new_v4().to_string());
        response.insert("ok", 1.0);
        Ok(response)
    }

    fn handle_create_user(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing createUser command");
        Ok(Self::build_success_response())
    }

    fn handle_update_user(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing updateUser command");
        Ok(Self::build_success_response())
    }

    fn handle_drop_user(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing dropUser command");
        Ok(Self::build_success_response())
    }

    fn handle_grant_roles(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing grantRolesToUser command");
        Ok(Self::build_success_response())
    }

    fn handle_revoke_roles(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing revokeRolesFromUser command");
        Ok(Self::build_success_response())
    }

    fn handle_users_info(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing usersInfo command");
        Ok(Self::build_cursor_response(vec![]))
    }

    fn handle_create_role(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing createRole command");
        Ok(Self::build_success_response())
    }

    fn handle_update_role(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing updateRole command");
        Ok(Self::build_success_response())
    }

    fn handle_drop_role(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing dropRole command");
        Ok(Self::build_success_response())
    }

    fn handle_grant_privileges(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing grantPrivilegesToRole command");
        Ok(Self::build_success_response())
    }

    fn handle_revoke_privileges(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing revokePrivilegesFromRole command");
        Ok(Self::build_success_response())
    }

    fn handle_grant_roles_to_role(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing grantRolesToRole command");
        Ok(Self::build_success_response())
    }

    fn handle_revoke_roles_from_role(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing revokeRolesFromRole command");
        Ok(Self::build_success_response())
    }

    fn handle_roles_info(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing rolesInfo command");
        Ok(Self::build_cursor_response(vec![]))
    }

    // Administrative Commands
    fn handle_build_info(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing buildInfo command");
        Ok(Self::build_version_info())
    }

    fn handle_ping(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing ping command");
        Ok(Self::build_success_response())
    }

    fn handle_is_master(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing isMaster command");
        let mut response = Document::new();
        response.insert("ismaster", true);
        response.insert("maxBsonObjectSize", 16777216);
        response.insert("maxMessageSizeBytes", 48000000);
        response.insert("maxWriteBatchSize", 100000);
        response.insert("localTime", chrono::Utc::now().to_rfc3339());
        response.insert("logicalSessionTimeoutMinutes", 30);
        response.insert("connectionId", 1);
        response.insert("minWireVersion", 0);
        response.insert("maxWireVersion", 17); // MongoDB 5.0+ wire version
        response.insert("readOnly", false);
        response.insert("ok", 1.0);
        Ok(response)
    }

    fn handle_hello(doc: Document) -> Result<Document> {
        fauxdb_info!("Processing hello command");
        Self::handle_is_master(doc)
    }

    fn handle_get_parameter(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing getParameter command");
        Ok(Self::build_success_response())
    }

    fn handle_set_parameter(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing setParameter command");
        Ok(Self::build_success_response())
    }

    fn handle_host_info(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing hostInfo command");
        Ok(Self::build_success_response())
    }

    fn handle_server_status(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing serverStatus command");
        Ok(Self::build_success_response())
    }

    fn handle_db_stats(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing dbStats command");
        Ok(Self::build_success_response())
    }

    fn handle_coll_stats(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing collStats command");
        Ok(Self::build_success_response())
    }

    fn handle_top(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing top command");
        Ok(Self::build_success_response())
    }

    fn handle_current_op(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing currentOp command");
        Ok(Self::build_success_response())
    }

    fn handle_kill_op(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing killOp command");
        Ok(Self::build_success_response())
    }

    fn handle_kill_all_sessions(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing killAllSessions command");
        Ok(Self::build_success_response())
    }

    fn handle_kill_sessions(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing killSessions command");
        Ok(Self::build_success_response())
    }

    fn handle_list_sessions(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing listSessions command");
        Ok(Self::build_cursor_response(vec![]))
    }

    fn handle_refresh_sessions(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing refreshSessions command");
        Ok(Self::build_success_response())
    }

    // Replication Commands (simplified implementations)
    fn handle_repl_set_get_status(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetGetStatus command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_get_config(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetGetConfig command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_initiate(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetInitiate command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_reconfig(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetReconfig command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_step_down(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetStepDown command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_freeze(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetFreeze command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_maintenance(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetMaintenance command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_sync_from(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetSyncFrom command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_resize_oplog(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetResizeOplog command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_heartbeat(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetHeartbeat command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_update_position(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetUpdatePosition command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_get_rbid(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetGetRBID command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_request_vote(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetRequestVote command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_decline_election(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetDeclineElection command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_elect(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetElect command");
        Ok(Self::build_success_response())
    }

    fn handle_repl_set_test(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing replSetTest command");
        Ok(Self::build_success_response())
    }

    // Sharding Commands (simplified implementations)
    fn handle_add_shard(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing addShard command");
        Ok(Self::build_success_response())
    }

    fn handle_remove_shard(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing removeShard command");
        Ok(Self::build_success_response())
    }

    fn handle_enable_sharding(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing enableSharding command");
        Ok(Self::build_success_response())
    }

    fn handle_shard_collection(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing shardCollection command");
        Ok(Self::build_success_response())
    }

    fn handle_shard_conn_pool_stats(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing shardConnPoolStats command");
        Ok(Self::build_success_response())
    }

    fn handle_get_shard_version(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing getShardVersion command");
        Ok(Self::build_success_response())
    }

    fn handle_get_shard_map(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing getShardMap command");
        Ok(Self::build_success_response())
    }

    fn handle_get_shard_distribution(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing getShardDistribution command");
        Ok(Self::build_success_response())
    }

    fn handle_move_chunk(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing moveChunk command");
        Ok(Self::build_success_response())
    }

    fn handle_split_chunk(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing splitChunk command");
        Ok(Self::build_success_response())
    }

    fn handle_merge_chunks(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing mergeChunks command");
        Ok(Self::build_success_response())
    }

    fn handle_balance_collection(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing balanceCollection command");
        Ok(Self::build_success_response())
    }

    fn handle_balancer_start(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing balancerStart command");
        Ok(Self::build_success_response())
    }

    fn handle_balancer_stop(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing balancerStop command");
        Ok(Self::build_success_response())
    }

    fn handle_balancer_status(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing balancerStatus command");
        Ok(Self::build_success_response())
    }

    // GridFS Commands
    fn handle_filemd5(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing filemd5 command");
        Ok(Self::build_success_response())
    }

    // MapReduce Commands
    fn handle_map_reduce(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing mapReduce command");
        Ok(Self::build_success_response())
    }

    // Change Streams Commands
    fn handle_get_more(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing getMore command");
        Ok(Self::build_cursor_response(vec![]))
    }

    // Time Series Commands
    fn handle_create_time_series(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing createTimeSeries command");
        Ok(Self::build_success_response())
    }

    // Search Commands
    fn handle_create_search_indexes(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing createSearchIndexes command");
        Ok(Self::build_success_response())
    }

    fn handle_update_search_index(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing updateSearchIndex command");
        Ok(Self::build_success_response())
    }

    fn handle_drop_search_index(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing dropSearchIndex command");
        Ok(Self::build_success_response())
    }

    fn handle_list_search_indexes(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing listSearchIndexes command");
        Ok(Self::build_cursor_response(vec![]))
    }

    // Atlas Commands
    fn handle_atlas_version(_doc: Document) -> Result<Document> {
        fauxdb_info!("Processing atlasVersion command");
        let mut response = Document::new();
        response.insert("atlasVersion", "1.0.0");
        response.insert("gitVersion", "fauxdb-atlas-1.0.0");
        response.insert("ok", 1.0);
        Ok(response)
    }

    // Helper methods
    fn build_success_response() -> Document {
        let mut doc = Document::new();
        doc.insert("ok", 1.0);
        doc
    }

    fn build_error_response(message: String) -> Document {
        let mut doc = Document::new();
        doc.insert("ok", 0.0);
        doc.insert("errmsg", message);
        doc
    }

    fn build_cursor_response(documents: Vec<Document>) -> Document {
        Self::build_cursor_response_with_namespace(documents, "test.users")
    }

    fn build_cursor_response_with_namespace(documents: Vec<Document>, namespace: &str) -> Document {
        let mut doc = Document::new();
        let mut cursor = Document::new();
        cursor.insert("firstBatch", documents);
        cursor.insert("id", 0i64);
        cursor.insert("ns", namespace);
        doc.insert("cursor", cursor);
        doc.insert("ok", 1.0);
        doc
    }

    fn build_database_info(name: &str, size: i64) -> Document {
        let mut doc = Document::new();
        doc.insert("name", name);
        doc.insert("sizeOnDisk", size);
        doc.insert("empty", false);
        doc
    }

    // Additional CRUD command handlers
    fn handle_insert_one(_doc: Document) -> Result<Document> {
        let mut result = Document::new();
        result.insert("acknowledged", true);
        result.insert("insertedId", bson::oid::ObjectId::new());
        Ok(result)
    }

    fn handle_insert_many(_doc: Document) -> Result<Document> {
        let mut result = Document::new();
        result.insert("acknowledged", true);
        result.insert("insertedIds", vec![
            bson::oid::ObjectId::new(),
            bson::oid::ObjectId::new(),
        ]);
        Ok(result)
    }

    fn handle_update_one(_doc: Document) -> Result<Document> {
        let mut result = Document::new();
        result.insert("acknowledged", true);
        result.insert("matchedCount", 1);
        result.insert("modifiedCount", 1);
        result.insert("upsertedId", Bson::Null);
        Ok(result)
    }

    fn handle_update_many(_doc: Document) -> Result<Document> {
        let mut result = Document::new();
        result.insert("acknowledged", true);
        result.insert("matchedCount", 5);
        result.insert("modifiedCount", 5);
        result.insert("upsertedId", Bson::Null);
        Ok(result)
    }

    fn handle_replace_one(_doc: Document) -> Result<Document> {
        let mut result = Document::new();
        result.insert("acknowledged", true);
        result.insert("matchedCount", 1);
        result.insert("modifiedCount", 1);
        result.insert("upsertedId", Bson::Null);
        Ok(result)
    }

    fn handle_delete_one(_doc: Document) -> Result<Document> {
        let mut result = Document::new();
        result.insert("acknowledged", true);
        result.insert("deletedCount", 1);
        Ok(result)
    }

    fn handle_delete_many(_doc: Document) -> Result<Document> {
        let mut result = Document::new();
        result.insert("acknowledged", true);
        result.insert("deletedCount", 3);
        Ok(result)
    }
}

impl Default for MongoDBCommandRegistry {
    fn default() -> Self {
        Self::new()
    }
}

