db.runCommand({delete: "test", deletes: [{q: {name: "test_item"}, limit: 1}]})
