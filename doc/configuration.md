# Configuration Guide

## Configuration File Format

FauxDB uses an INI-style configuration file with sections and key-value pairs. The default location is `/etc/fauxdb/fauxdb.conf`.

## Sample Configuration

```ini
# ====================================
# FauxDB Configuration File
# ====================================

[server]
# Network settings
port = 27018
bind_address = 127.0.0.1
max_connections = 1000
keepalive_timeout = 300

# Runtime mode
daemon_mode = false
user = fauxdb
group = fauxdb

[postgresql]
# Database connection
host = localhost
port = 5432
database = fauxdb
user = fauxdb
password = your_secure_password

# Connection pooling
pool_size = 20
pool_min_size = 5
pool_max_size = 100
pool_timeout = 30

# Connection options
connect_timeout = 10
query_timeout = 300
ssl_mode = prefer

[logging]
# Log files
log_file = /var/log/fauxdb/fauxdb.log
error_log = /var/log/fauxdb/error.log
access_log = /var/log/fauxdb/access.log

# Log levels: 0=emerg, 1=alert, 2=crit, 3=err, 4=warn, 5=notice, 6=info, 7=debug
log_level = 6
error_log_level = 3

# Log formatting
log_format = timestamp
log_rotation = daily
log_max_size = 100MB
log_max_files = 10

[security]
# Authentication
auth_enabled = false
auth_database = admin
auth_mechanism = SCRAM-SHA-256

# SSL/TLS
ssl_enabled = false
ssl_cert_file = /etc/fauxdb/server.crt
ssl_key_file = /etc/fauxdb/server.key
ssl_ca_file = /etc/fauxdb/ca.crt

[performance]
# Query optimization
query_cache_enabled = true
query_cache_size = 128MB
query_cache_ttl = 300

# Worker threads
worker_threads = 0  # 0 = auto-detect CPU cores
io_threads = 4

# Memory limits
max_memory_usage = 1GB
max_document_size = 16MB

[monitoring]
# Health checks
health_check_enabled = true
health_check_port = 27019
health_check_path = /health

# Metrics
metrics_enabled = false
metrics_port = 9090
metrics_path = /metrics

[debug]
# Debug settings
debug_enabled = false
debug_level = 1
debug_modules = query,protocol
debug_output = file
```

## Configuration Sections

### [server] Section

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `port` | integer | 27018 | Port to listen on |
| `bind_address` | string | 127.0.0.1 | IP address to bind to |
| `max_connections` | integer | 1000 | Maximum concurrent connections |
| `keepalive_timeout` | integer | 300 | Connection keepalive timeout (seconds) |
| `daemon_mode` | boolean | false | Run as daemon process |
| `user` | string | fauxdb | User to run as (daemon mode) |
| `group` | string | fauxdb | Group to run as (daemon mode) |

### [postgresql] Section

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `host` | string | localhost | PostgreSQL server hostname |
| `port` | integer | 5432 | PostgreSQL server port |
| `database` | string | fauxdb | Database name |
| `user` | string | fauxdb | Database username |
| `password` | string | | Database password |
| `pool_size` | integer | 10 | Initial connection pool size |
| `pool_min_size` | integer | 5 | Minimum pool size |
| `pool_max_size` | integer | 100 | Maximum pool size |
| `pool_timeout` | integer | 30 | Pool connection timeout (seconds) |
| `connect_timeout` | integer | 10 | Database connection timeout |
| `query_timeout` | integer | 300 | Query execution timeout |
| `ssl_mode` | string | prefer | SSL mode (disable/allow/prefer/require) |

### [logging] Section

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `log_file` | string | /var/log/fauxdb/fauxdb.log | Main log file path |
| `error_log` | string | /var/log/fauxdb/error.log | Error log file path |
| `access_log` | string | /var/log/fauxdb/access.log | Access log file path |
| `log_level` | integer | 6 | Log level (0-7) |
| `error_log_level` | integer | 3 | Error log level (0-7) |
| `log_format` | string | timestamp | Log format (simple/timestamp/json) |
| `log_rotation` | string | daily | Rotation frequency (hourly/daily/weekly) |
| `log_max_size` | string | 100MB | Maximum log file size |
| `log_max_files` | integer | 10 | Maximum number of rotated files |

## Command Line Override

Configuration values can be overridden via command line arguments:

```bash
# Override port and bind address
./fauxdb --config /etc/fauxdb/fauxdb.conf --port 27017 --bind 0.0.0.0

# Enable debug mode
./fauxdb --debug --log-level 7

# Run as daemon with custom user
./fauxdb --daemon --user myuser --group mygroup
```

## Environment Variables

Configuration can also be set via environment variables:

```bash
export FAUXDB_PORT=27017
export FAUXDB_BIND_ADDRESS=0.0.0.0
export FAUXDB_PG_HOST=db.example.com
export FAUXDB_PG_PASSWORD=secret123
export FAUXDB_LOG_LEVEL=7
export FAUXDB_DEBUG=true

./fauxdb
```

### Environment Variable Naming

Environment variables follow the pattern: `FAUXDB_<SECTION>_<OPTION>` (uppercase, with underscores).

Examples:
- `server.port` → `FAUXDB_SERVER_PORT`
- `postgresql.host` → `FAUXDB_POSTGRESQL_HOST`
- `logging.log_level` → `FAUXDB_LOGGING_LOG_LEVEL`

## Configuration Validation

FauxDB validates configuration on startup:

```bash
# Test configuration
./fauxdb --config /etc/fauxdb/fauxdb.conf --test-config

# Validate without starting
./fauxdb --validate-only
```

## Production Configuration Examples

### High Performance Setup
```ini
[server]
port = 27018
bind_address = 0.0.0.0
max_connections = 5000

[postgresql]
host = db-primary.internal
port = 5432
pool_size = 50
pool_max_size = 200
query_timeout = 60

[performance]
worker_threads = 16
query_cache_enabled = true
query_cache_size = 512MB

[logging]
log_level = 4  # warnings and above
log_rotation = hourly
```

### Development Setup
```ini
[server]
port = 27018
bind_address = 127.0.0.1
max_connections = 100

[postgresql]
host = localhost
database = fauxdb_dev
pool_size = 5

[logging]
log_level = 7  # debug
log_format = timestamp

[debug]
debug_enabled = true
debug_level = 2
debug_modules = query,protocol,database
```

### Secure Production Setup
```ini
[server]
bind_address = 10.0.1.100
max_connections = 2000

[postgresql]
host = db.internal
ssl_mode = require
connect_timeout = 5

[security]
auth_enabled = true
ssl_enabled = true
ssl_cert_file = /etc/ssl/certs/fauxdb.crt
ssl_key_file = /etc/ssl/private/fauxdb.key

[logging]
log_level = 5  # notice and above
log_format = json
access_log = /var/log/fauxdb/access.log
```

## Configuration Hot Reload

FauxDB supports configuration reload without restart:

```bash
# Send SIGHUP to reload configuration
sudo kill -HUP $(cat /var/run/fauxdb/fauxdb.pid)

# Or use systemctl
sudo systemctl reload fauxdb

# Or use the management interface
curl -X POST http://localhost:27019/admin/reload
```

### Reloadable Settings

The following settings can be reloaded without restart:
- Log levels and file paths
- Connection pool sizes
- Query cache settings
- Debug settings
- Monitoring settings

### Non-reloadable Settings

These settings require a restart:
- Server port and bind address
- User and group (daemon mode)
- SSL certificate files
- Authentication settings

## Security Configuration

### File Permissions
```bash
# Secure the configuration file
sudo chown root:fauxdb /etc/fauxdb/fauxdb.conf
sudo chmod 640 /etc/fauxdb/fauxdb.conf

# Secure SSL files
sudo chown root:fauxdb /etc/fauxdb/*.key
sudo chmod 600 /etc/fauxdb/*.key
```

### PostgreSQL Security
```ini
[postgresql]
# Use connection strings for complex authentication
connection_string = "host=localhost dbname=fauxdb user=fauxdb sslmode=require sslcert=/etc/ssl/client.crt sslkey=/etc/ssl/client.key"

# Or individual settings
ssl_mode = require
ssl_cert_file = /etc/ssl/certs/client.crt
ssl_key_file = /etc/ssl/private/client.key
ssl_ca_file = /etc/ssl/certs/ca.crt
```

## Troubleshooting Configuration

### Common Issues

#### Permission Denied
```bash
# Check file permissions
ls -la /etc/fauxdb/fauxdb.conf

# Fix permissions
sudo chown fauxdb:fauxdb /etc/fauxdb/fauxdb.conf
sudo chmod 644 /etc/fauxdb/fauxdb.conf
```

#### Invalid Configuration
```bash
# Validate configuration
./fauxdb --test-config --config /etc/fauxdb/fauxdb.conf

# Check syntax
grep -n "=" /etc/fauxdb/fauxdb.conf | grep -v "^#"
```

#### PostgreSQL Connection Issues
```bash
# Test database connection manually
psql "host=localhost dbname=fauxdb user=fauxdb sslmode=require"

# Check connection string
./fauxdb --test-db-connection
```

### Configuration Debugging

Enable configuration debugging:
```ini
[debug]
debug_enabled = true
debug_modules = config
```

This will log all configuration loading and parsing steps.

## Advanced Configuration

### Custom Schema Mapping
```ini
[schema]
# Map MongoDB collections to PostgreSQL tables
users = user_profiles
orders = customer_orders
products = inventory_items

# Default table prefix
table_prefix = mongo_
```

### Query Translation Options
```ini
[translation]
# Enable advanced query features
enable_aggregation = true
enable_text_search = true
enable_geospatial = true

# Query optimization
optimize_joins = true
push_down_filters = true
use_materialized_views = true
```

### Monitoring Integration
```ini
[monitoring]
# Prometheus metrics
prometheus_enabled = true
prometheus_port = 9090

# Health checks
health_check_enabled = true
health_check_interval = 30

# Performance monitoring
slow_query_threshold = 1000  # milliseconds
log_slow_queries = true
```

## Next Steps

- [Usage Guide](usage.md) - Learn how to use FauxDB
- [Architecture Overview](architecture.md) - Understand the internal structure
- [Performance Tuning](performance.md) - Optimize for your workload
- [Monitoring Setup](monitoring.md) - Set up monitoring and alerting
