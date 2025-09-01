db.runCommand({update: "test", updates: [{q: {name: "test"}, u: {$set: {updated: true}}}]})
