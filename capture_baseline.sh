#!/bin/bash

# Capture current MongoDB command outputs as the baseline expected results

echo "Capturing current FauxDB outputs as baseline expected results..."

# Check if server is running
if ! mongosh --port 27018 --quiet --eval "db.runCommand({ping: 1})" > /dev/null 2>&1; then
    echo "FauxDB server is not running on port 27018"
    echo "Please start with: ./fauxdb --config config/fauxdb.json"
    exit 1
fi

mkdir -p tests/expected

# Capture each command output
echo "Capturing ping..."
mongosh --port 27018 --quiet --eval 'db.runCommand({ping: 1})' > tests/expected/ping.json

echo "Capturing hello..."
mongosh --port 27018 --quiet --eval 'db.runCommand({hello: 1})' > tests/expected/hello.json

echo "Capturing buildInfo..."
mongosh --port 27018 --quiet --eval 'db.runCommand({buildInfo: 1})' > tests/expected/buildInfo.json

echo "Capturing count..."
mongosh --port 27018 --quiet --eval 'db.runCommand({count: "test"})' > tests/expected/count.json

echo "Capturing countDocuments..."
mongosh --port 27018 --quiet --eval 'db.runCommand({countDocuments: "test"})' > tests/expected/countDocuments.json

echo "Capturing find_basic..."
mongosh --port 27018 --quiet --eval 'db.runCommand({find: "test", limit: 5})' > tests/expected/find_basic.json

echo "Capturing findOne..."
mongosh --port 27018 --quiet --eval 'db.runCommand({findOne: "test"})' > tests/expected/findOne.json

echo "Capturing insert..."
mongosh --port 27018 --quiet --eval 'db.runCommand({insert: "test", documents: [{name: "new_item", value: 999}]})' > tests/expected/insert.json

echo "Capturing update..."
mongosh --port 27018 --quiet --eval 'db.runCommand({update: "test", updates: [{q: {name: "test"}, u: {$set: {updated: true}}}]})' > tests/expected/update.json

echo "Capturing listCollections..."
mongosh --port 27018 --quiet --eval 'db.runCommand({listCollections: 1})' > tests/expected/listCollections.json

echo "Capturing listDatabases..."
mongosh --port 27018 --quiet --eval 'db.runCommand({listDatabases: 1})' > tests/expected/listDatabases.json

echo "Capturing aggregate_empty..."
mongosh --port 27018 --quiet --eval 'db.runCommand({aggregate: "test", pipeline: []})' > tests/expected/aggregate_empty.json

echo "Capturing unknown_command..."
mongosh --port 27018 --quiet --eval 'db.runCommand({unknownCommand: 1})' > tests/expected/unknown_command.json

echo "Baseline expected results captured!"
echo "All expected/* files now contain the current FauxDB output format."
