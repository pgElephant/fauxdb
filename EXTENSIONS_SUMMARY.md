# FauxDB Extensions Summary

## Standard PostgreSQL Extensions (Included with PostgreSQL)

These extensions are part of the standard PostgreSQL distribution and are installed automatically by the build script:

- **btree_gin**: GIN index support for B-tree operations
- **btree_gist**: GiST index support for B-tree operations  
- **pg_trgm**: Trigram matching for text search
- **plpython3u**: Python procedural language support

## Third-Party Extensions (Manual Installation Required)

Only one third-party extension is required for FauxDB:

- **DocumentDB Extension**: MongoDB-compatible query language for JSONB
  - Installation: Manual (see `docs/guides/documentdb-extension.md`)
  - Purpose: Enhanced MongoDB-style operations on JSONB data
  - Repository: Third-party DocumentDB compatibility layer

## Installation Process

### Automatic (Build Script)
```bash
./build.sh  # Installs standard PostgreSQL extensions only
```

### Manual (Documentation Guide)
```bash
# Follow: docs/guides/documentdb-extension.md
# Installs only the DocumentDB third-party extension
```

## Verification

### Check Standard Extensions
```sql
SELECT extname FROM pg_extension 
WHERE extname IN ('btree_gin', 'btree_gist', 'pg_trgm', 'plpython3u');
```

### Check Third-Party Extension
```sql
SELECT extname FROM pg_extension WHERE extname = 'documentdb';
```

## Summary

- **Standard Extensions**: 4 (installed automatically)
- **Third-Party Extensions**: 1 (DocumentDB only)
- **Total Extensions**: 5

No other third-party extensions are installed or required for FauxDB operation.
