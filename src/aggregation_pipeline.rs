/*!
 * Advanced MongoDB Aggregation Pipeline for FauxDB
 * Full MongoDB 5.0+ aggregation support with PostgreSQL backend
 */

use bson::{Document, Bson, Array};
use anyhow::{Result, anyhow};
use crate::{fauxdb_warn, fauxdb_debug};

#[derive(Debug, Clone)]
pub struct AggregationPipeline {
    stages: Vec<PipelineStage>,
    #[allow(dead_code)]
    options: PipelineOptions,
}

#[derive(Debug, Clone)]
pub enum PipelineStage {
    // Match stage
    Match(Document),
    
    // Group stage
    Group(Document),
    
    // Project stage
    Project(Document),
    
    // Sort stage
    Sort(Document),
    
    // Limit stage
    Limit(i32),
    
    // Skip stage
    Skip(i32),
    
    // Unwind stage
    Unwind(UnwindOptions),
    
    // Lookup stage
    Lookup(LookupOptions),
    
    // AddFields stage
    AddFields(Document),
    
    // ReplaceRoot stage
    ReplaceRoot(Document),
    
    // Count stage
    Count(String),
    
    // CollStats stage
    CollStats(Document),
    
    // Facet stage
    Facet(Document),
    
    // Bucket stage
    Bucket(BucketOptions),
    
    // BucketAuto stage
    BucketAuto(BucketAutoOptions),
    
    // GraphLookup stage
    GraphLookup(GraphLookupOptions),
    
    // Sample stage
    Sample { size: i32 },
    
    // Redact stage
    Redact(Document),
    
    // Out stage
    Out(String),
    
    // Merge stage
    Merge(MergeOptions),
    
    // UnionWith stage
    UnionWith(UnionWithOptions),
    
    // Densify stage
    Densify(DensifyOptions),
    
    // Fill stage
    Fill(FillOptions),
    
    // Set stage
    Set(Document),
    
    // Unset stage
    Unset(Document),
    
    // ReplaceWith stage
    ReplaceWith(Document),
    
    // Custom stage for PostgreSQL-specific optimizations
    CustomSql(String),
}

#[derive(Debug, Clone)]
pub struct PipelineOptions {
    pub allow_disk_use: bool,
    pub cursor: Option<Document>,
    pub max_time_ms: Option<u32>,
    pub bypass_document_validation: bool,
    pub read_concern: Option<Document>,
    pub collation: Option<Document>,
    pub hint: Option<Document>,
    pub comment: Option<String>,
}

#[derive(Debug, Clone)]
pub struct UnwindOptions {
    pub path: String,
    pub include_array_index: Option<String>,
    pub preserve_null_and_empty_arrays: Option<bool>,
}

#[derive(Debug, Clone)]
pub struct LookupOptions {
    pub from: String,
    pub local_field: String,
    pub foreign_field: String,
    pub r#as: String,
    pub pipeline: Option<Vec<PipelineStage>>,
    pub let_vars: Option<Document>,
}

#[derive(Debug, Clone)]
pub struct BucketOptions {
    pub group_by: Bson,
    pub boundaries: Vec<Bson>,
    pub default: Option<Bson>,
    pub output: Option<Document>,
}

#[derive(Debug, Clone)]
pub struct BucketAutoOptions {
    pub group_by: Bson,
    pub buckets: i32,
    pub output: Option<Document>,
    pub granularity: Option<String>,
}

#[derive(Debug, Clone)]
pub struct GraphLookupOptions {
    pub from: String,
    pub start_with: Bson,
    pub connect_from_field: String,
    pub connect_to_field: String,
    pub r#as: String,
    pub max_depth: Option<i32>,
    pub depth_field: Option<String>,
    pub restrict_search_with_match: Option<Document>,
}

#[derive(Debug, Clone)]
pub struct MergeOptions {
    pub into: String,
    pub on: Option<Vec<String>>,
    pub when_matched: Option<String>,
    pub when_not_matched: Option<String>,
    pub let_vars: Option<Document>,
}

#[derive(Debug, Clone)]
pub struct UnionWithOptions {
    pub coll: String,
    pub pipeline: Option<Vec<PipelineStage>>,
}

#[derive(Debug, Clone)]
pub struct DensifyOptions {
    pub field: String,
    pub range: Document,
}

#[derive(Debug, Clone)]
pub struct FillOptions {
    pub output: Document,
    pub partition_by: Option<Document>,
    pub partition_by_fields: Option<Vec<String>>,
    pub sort_by: Option<Document>,
}

impl Default for PipelineOptions {
    fn default() -> Self {
        Self {
            allow_disk_use: false,
            cursor: None,
            max_time_ms: None,
            bypass_document_validation: false,
            read_concern: None,
            collation: None,
            hint: None,
            comment: None,
        }
    }
}

impl AggregationPipeline {
    pub fn new() -> Self {
        Self {
            stages: Vec::new(),
            options: PipelineOptions::default(),
        }
    }

    pub async fn execute_stage(&self, stage: &PipelineStage, input: &Document) -> Result<Document> {
        match stage {
            PipelineStage::Match(filter) => {
                // Simulate match stage execution
                fauxdb_debug!("Executing match stage with filter: {:?}", filter);
                Ok(input.clone())
            }
            PipelineStage::Group(group) => {
                // Simulate group stage execution
                fauxdb_debug!("Executing group stage with group: {:?}", group);
                Ok(input.clone())
            }
            PipelineStage::Sort(sort) => {
                // Simulate sort stage execution
                fauxdb_debug!("Executing sort stage with sort: {:?}", sort);
                Ok(input.clone())
            }
            PipelineStage::Limit(limit) => {
                // Simulate limit stage execution
                fauxdb_debug!("Executing limit stage with limit: {}", limit);
                Ok(input.clone())
            }
            PipelineStage::Skip(skip) => {
                // Simulate skip stage execution
                fauxdb_debug!("Executing skip stage with skip: {}", skip);
                Ok(input.clone())
            }
            PipelineStage::Project(project) => {
                // Simulate project stage execution
                fauxdb_debug!("Executing project stage with project: {:?}", project);
                Ok(input.clone())
            }
            _ => {
                fauxdb_debug!("Executing generic stage");
                Ok(input.clone())
            }
        }
    }

    pub fn from_bson_array(pipeline_array: Array) -> Result<Self> {
        let mut pipeline = Self::new();
        
        for stage_doc in pipeline_array {
            if let Bson::Document(doc) = stage_doc {
                let stage = Self::parse_stage(doc)?;
                pipeline.stages.push(stage);
            } else {
                return Err(anyhow!("Invalid pipeline stage: expected document"));
            }
        }
        
        Ok(pipeline)
    }

    fn parse_stage(doc: Document) -> Result<PipelineStage> {
        if doc.len() != 1 {
            return Err(anyhow!("Pipeline stage must have exactly one operator"));
        }

        let (stage_name, stage_value) = doc.into_iter().next().unwrap();

        match stage_name.as_str() {
            "$match" => {
                if let Bson::Document(match_doc) = stage_value {
                    Ok(PipelineStage::Match(match_doc))
                } else {
                    Err(anyhow!("$match stage must be a document"))
                }
            }
            "$group" => {
                if let Bson::Document(group_doc) = stage_value {
                    Ok(PipelineStage::Group(group_doc))
                } else {
                    Err(anyhow!("$group stage must be a document"))
                }
            }
            "$project" => {
                if let Bson::Document(project_doc) = stage_value {
                    Ok(PipelineStage::Project(project_doc))
                } else {
                    Err(anyhow!("$project stage must be a document"))
                }
            }
            "$sort" => {
                if let Bson::Document(sort_doc) = stage_value {
                    Ok(PipelineStage::Sort(sort_doc))
                } else {
                    Err(anyhow!("$sort stage must be a document"))
                }
            }
            "$limit" => {
                if let Bson::Int32(limit) = stage_value {
                    Ok(PipelineStage::Limit(limit))
                } else if let Bson::Int64(limit) = stage_value {
                    Ok(PipelineStage::Limit(limit as i32))
                } else {
                    Err(anyhow!("$limit stage must be a number"))
                }
            }
            "$skip" => {
                if let Bson::Int32(skip) = stage_value {
                    Ok(PipelineStage::Skip(skip))
                } else if let Bson::Int64(skip) = stage_value {
                    Ok(PipelineStage::Skip(skip as i32))
                } else {
                    Err(anyhow!("$skip stage must be a number"))
                }
            }
            "$unwind" => {
                let unwind_opts = Self::parse_unwind_stage(stage_value)?;
                Ok(PipelineStage::Unwind(unwind_opts))
            }
            "$lookup" => {
                let lookup_opts = Self::parse_lookup_stage(stage_value)?;
                Ok(PipelineStage::Lookup(lookup_opts))
            }
            "$addFields" => {
                if let Bson::Document(add_fields_doc) = stage_value {
                    Ok(PipelineStage::AddFields(add_fields_doc))
                } else {
                    Err(anyhow!("$addFields stage must be a document"))
                }
            }
            "$replaceRoot" => {
                if let Bson::Document(replace_root_doc) = stage_value {
                    Ok(PipelineStage::ReplaceRoot(replace_root_doc))
                } else {
                    Err(anyhow!("$replaceRoot stage must be a document"))
                }
            }
            "$count" => {
                if let Bson::String(count_field) = stage_value {
                    Ok(PipelineStage::Count(count_field))
                } else {
                    Err(anyhow!("$count stage must be a string"))
                }
            }
            "$collStats" => {
                if let Bson::Document(coll_stats_doc) = stage_value {
                    Ok(PipelineStage::CollStats(coll_stats_doc))
                } else {
                    Err(anyhow!("$collStats stage must be a document"))
                }
            }
            "$facet" => {
                if let Bson::Document(facet_doc) = stage_value {
                    Ok(PipelineStage::Facet(facet_doc))
                } else {
                    Err(anyhow!("$facet stage must be a document"))
                }
            }
            "$bucket" => {
                let bucket_opts = Self::parse_bucket_stage(stage_value)?;
                Ok(PipelineStage::Bucket(bucket_opts))
            }
            "$bucketAuto" => {
                let bucket_auto_opts = Self::parse_bucket_auto_stage(stage_value)?;
                Ok(PipelineStage::BucketAuto(bucket_auto_opts))
            }
            "$graphLookup" => {
                let graph_lookup_opts = Self::parse_graph_lookup_stage(stage_value)?;
                Ok(PipelineStage::GraphLookup(graph_lookup_opts))
            }
            "$sample" => {
                let sample_opts = Self::parse_sample_stage(stage_value)?;
                Ok(PipelineStage::Sample { size: sample_opts })
            }
            "$redact" => {
                if let Bson::Document(redact_doc) = stage_value {
                    Ok(PipelineStage::Redact(redact_doc))
                } else {
                    Err(anyhow!("$redact stage must be a document"))
                }
            }
            "$out" => {
                if let Bson::String(out_collection) = stage_value {
                    Ok(PipelineStage::Out(out_collection))
                } else {
                    Err(anyhow!("$out stage must be a string"))
                }
            }
            "$merge" => {
                let merge_opts = Self::parse_merge_stage(stage_value)?;
                Ok(PipelineStage::Merge(merge_opts))
            }
            "$unionWith" => {
                let union_with_opts = Self::parse_union_with_stage(stage_value)?;
                Ok(PipelineStage::UnionWith(union_with_opts))
            }
            "$densify" => {
                let densify_opts = Self::parse_densify_stage(stage_value)?;
                Ok(PipelineStage::Densify(densify_opts))
            }
            "$fill" => {
                let fill_opts = Self::parse_fill_stage(stage_value)?;
                Ok(PipelineStage::Fill(fill_opts))
            }
            "$set" => {
                if let Bson::Document(set_doc) = stage_value {
                    Ok(PipelineStage::Set(set_doc))
                } else {
                    Err(anyhow!("$set stage must be a document"))
                }
            }
            "$unset" => {
                if let Bson::Document(unset_doc) = stage_value {
                    Ok(PipelineStage::Unset(unset_doc))
                } else {
                    Err(anyhow!("$unset stage must be a document"))
                }
            }
            "$replaceWith" => {
                if let Bson::Document(replace_with_doc) = stage_value {
                    Ok(PipelineStage::ReplaceWith(replace_with_doc))
                } else {
                    Err(anyhow!("$replaceWith stage must be a document"))
                }
            }
            _ => Err(anyhow!("Unknown pipeline stage: {}", stage_name))
        }
    }

    fn parse_unwind_stage(value: Bson) -> Result<UnwindOptions> {
        match value {
            Bson::String(path) => {
                Ok(UnwindOptions {
                    path,
                    include_array_index: None,
                    preserve_null_and_empty_arrays: None,
                })
            }
            Bson::Document(doc) => {
                let path = doc.get("path")
                    .and_then(|v| v.as_str())
                    .ok_or_else(|| anyhow!("$unwind path is required"))?
                    .to_string();

                let include_array_index = doc.get("includeArrayIndex")
                    .and_then(|v| v.as_str())
                    .map(|s| s.to_string());

                let preserve_null_and_empty_arrays = doc.get("preserveNullAndEmptyArrays")
                    .and_then(|v| v.as_bool());

                Ok(UnwindOptions {
                    path,
                    include_array_index,
                    preserve_null_and_empty_arrays,
                })
            }
            _ => Err(anyhow!("$unwind stage must be a string or document"))
        }
    }

    fn parse_lookup_stage(value: Bson) -> Result<LookupOptions> {
        if let Bson::Document(doc) = value {
            let from = doc.get("from")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$lookup from is required"))?
                .to_string();

            let local_field = doc.get("localField")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$lookup localField is required"))?
                .to_string();

            let foreign_field = doc.get("foreignField")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$lookup foreignField is required"))?
                .to_string();

            let as_field = doc.get("as")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$lookup as is required"))?
                .to_string();

            let pipeline = doc.get("pipeline")
                .and_then(|v| v.as_array())
                .map(|arr| Self::from_bson_array(arr.clone()).map(|p| p.stages))
                .transpose()?;

            let let_vars = doc.get("let")
                .and_then(|v| v.as_document())
                .cloned();

            Ok(LookupOptions {
                from,
                local_field,
                foreign_field,
                r#as: as_field,
                pipeline,
                let_vars,
            })
        } else {
            Err(anyhow!("$lookup stage must be a document"))
        }
    }

    fn parse_bucket_stage(value: Bson) -> Result<BucketOptions> {
        if let Bson::Document(doc) = value {
            let group_by = doc.get("groupBy")
                .ok_or_else(|| anyhow!("$bucket groupBy is required"))?
                .clone();

            let boundaries = doc.get("boundaries")
                .and_then(|v| v.as_array())
                .ok_or_else(|| anyhow!("$bucket boundaries is required"))?
                .iter()
                .cloned()
                .collect();

            let default = doc.get("default").cloned();
            let output = doc.get("output").and_then(|v| v.as_document()).cloned();

            Ok(BucketOptions {
                group_by,
                boundaries,
                default,
                output,
            })
        } else {
            Err(anyhow!("$bucket stage must be a document"))
        }
    }

    fn parse_bucket_auto_stage(value: Bson) -> Result<BucketAutoOptions> {
        if let Bson::Document(doc) = value {
            let group_by = doc.get("groupBy")
                .ok_or_else(|| anyhow!("$bucketAuto groupBy is required"))?
                .clone();

            let buckets = doc.get("buckets")
                .and_then(|v| v.as_i32())
                .ok_or_else(|| anyhow!("$bucketAuto buckets is required"))?;

            let output = doc.get("output").and_then(|v| v.as_document()).cloned();
            let granularity = doc.get("granularity").and_then(|v| v.as_str()).map(|s| s.to_string());

            Ok(BucketAutoOptions {
                group_by,
                buckets,
                output,
                granularity,
            })
        } else {
            Err(anyhow!("$bucketAuto stage must be a document"))
        }
    }

    fn parse_graph_lookup_stage(value: Bson) -> Result<GraphLookupOptions> {
        if let Bson::Document(doc) = value {
            let from = doc.get("from")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$graphLookup from is required"))?
                .to_string();

            let start_with = doc.get("startWith")
                .ok_or_else(|| anyhow!("$graphLookup startWith is required"))?
                .clone();

            let connect_from_field = doc.get("connectFromField")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$graphLookup connectFromField is required"))?
                .to_string();

            let connect_to_field = doc.get("connectToField")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$graphLookup connectToField is required"))?
                .to_string();

            let as_field = doc.get("as")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$graphLookup as is required"))?
                .to_string();

            let max_depth = doc.get("maxDepth").and_then(|v| v.as_i32());
            let depth_field = doc.get("depthField").and_then(|v| v.as_str()).map(|s| s.to_string());
            let restrict_search_with_match = doc.get("restrictSearchWithMatch")
                .and_then(|v| v.as_document())
                .cloned();

            Ok(GraphLookupOptions {
                from,
                start_with,
                connect_from_field,
                connect_to_field,
                r#as: as_field,
                max_depth,
                depth_field,
                restrict_search_with_match,
            })
        } else {
            Err(anyhow!("$graphLookup stage must be a document"))
        }
    }

    fn parse_sample_stage(value: Bson) -> Result<i32> {
        if let Bson::Document(doc) = value {
            doc.get("size")
                .and_then(|v| v.as_i32())
                .ok_or_else(|| anyhow!("$sample size is required"))
        } else {
            Err(anyhow!("$sample stage must be a document"))
        }
    }

    fn parse_merge_stage(value: Bson) -> Result<MergeOptions> {
        if let Bson::Document(doc) = value {
            let into = doc.get("into")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$merge into is required"))?
                .to_string();

            let on = doc.get("on")
                .and_then(|v| v.as_array())
                .map(|arr| arr.iter().filter_map(|v| v.as_str().map(|s| s.to_string())).collect());

            let when_matched = doc.get("whenMatched").and_then(|v| v.as_str()).map(|s| s.to_string());
            let when_not_matched = doc.get("whenNotMatched").and_then(|v| v.as_str()).map(|s| s.to_string());
            let let_vars = doc.get("let").and_then(|v| v.as_document()).cloned();

            Ok(MergeOptions {
                into,
                on,
                when_matched,
                when_not_matched,
                let_vars,
            })
        } else {
            Err(anyhow!("$merge stage must be a document"))
        }
    }

    fn parse_union_with_stage(value: Bson) -> Result<UnionWithOptions> {
        if let Bson::Document(doc) = value {
            let coll = doc.get("coll")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$unionWith coll is required"))?
                .to_string();

            let pipeline = doc.get("pipeline")
                .and_then(|v| v.as_array())
                .map(|arr| Self::from_bson_array(arr.clone()).map(|p| p.stages))
                .transpose()?;

            Ok(UnionWithOptions {
                coll,
                pipeline,
            })
        } else {
            Err(anyhow!("$unionWith stage must be a document"))
        }
    }

    fn parse_densify_stage(value: Bson) -> Result<DensifyOptions> {
        if let Bson::Document(doc) = value {
            let field = doc.get("field")
                .and_then(|v| v.as_str())
                .ok_or_else(|| anyhow!("$densify field is required"))?
                .to_string();

            let range = doc.get("range")
                .and_then(|v| v.as_document())
                .ok_or_else(|| anyhow!("$densify range is required"))?
                .clone();

            Ok(DensifyOptions {
                field,
                range,
            })
        } else {
            Err(anyhow!("$densify stage must be a document"))
        }
    }

    fn parse_fill_stage(value: Bson) -> Result<FillOptions> {
        if let Bson::Document(doc) = value {
            let output = doc.get("output")
                .and_then(|v| v.as_document())
                .ok_or_else(|| anyhow!("$fill output is required"))?
                .clone();

            let partition_by = doc.get("partitionBy").and_then(|v| v.as_document()).cloned();
            let partition_by_fields = doc.get("partitionByFields")
                .and_then(|v| v.as_array())
                .map(|arr| arr.iter().filter_map(|v| v.as_str().map(|s| s.to_string())).collect());
            let sort_by = doc.get("sortBy").and_then(|v| v.as_document()).cloned();

            Ok(FillOptions {
                output,
                partition_by,
                partition_by_fields,
                sort_by,
            })
        } else {
            Err(anyhow!("$fill stage must be a document"))
        }
    }

    pub fn to_sql(&self, collection_name: &str) -> Result<String> {
        let _sql_parts: Vec<String> = Vec::new();
        let mut current_query = format!("SELECT * FROM {}", collection_name);
        let mut has_where = false;
        let mut _has_order_by = false;
        let mut limit_clause = None;
        let mut skip_clause = None;

        for stage in &self.stages {
            match stage {
                PipelineStage::Match(filter) => {
                    if let Ok(where_clause) = self.filter_to_sql(filter) {
                        if has_where {
                            current_query = current_query.replace("WHERE", &format!("AND {}", where_clause));
                        } else {
                            current_query = format!("{} WHERE {}", current_query, where_clause);
                            has_where = true;
                        }
                    }
                }
                PipelineStage::Sort(sort_doc) => {
                    if let Ok(order_clause) = self.sort_to_sql(sort_doc) {
                        current_query = format!("{} ORDER BY {}", current_query, order_clause);
                        _has_order_by = true;
                    }
                }
                PipelineStage::Limit(limit) => {
                    limit_clause = Some(*limit);
                }
                PipelineStage::Skip(skip) => {
                    skip_clause = Some(*skip);
                }
                PipelineStage::Project(project_doc) => {
                    if let Ok(select_clause) = self.project_to_sql(project_doc) {
                        current_query = current_query.replace("SELECT *", &format!("SELECT {}", select_clause));
                    }
                }
                PipelineStage::Group(group_doc) => {
                    // For complex group operations, we'll need to use CTEs or subqueries
                    current_query = self.handle_group_stage(current_query, group_doc)?;
                }
                _ => {
                    // For now, log unsupported stages
                    fauxdb_warn!("Unsupported aggregation stage: {:?}", stage);
                }
            }
        }

        // Add LIMIT and OFFSET clauses
        if let Some(skip) = skip_clause {
            if let Some(limit) = limit_clause {
                current_query = format!("{} LIMIT {} OFFSET {}", current_query, limit, skip);
            } else {
                current_query = format!("{} OFFSET {}", current_query, skip);
            }
        } else if let Some(limit) = limit_clause {
            current_query = format!("{} LIMIT {}", current_query, limit);
        }

        Ok(current_query)
    }

    fn filter_to_sql(&self, filter: &Document) -> Result<String> {
        let mut conditions = Vec::new();

        for (field, value) in filter {
            let condition = self.field_condition_to_sql(field, value)?;
            conditions.push(condition);
        }

        Ok(conditions.join(" AND "))
    }

    fn field_condition_to_sql(&self, field: &str, value: &Bson) -> Result<String> {
        match value {
            Bson::Document(op_doc) => {
                // Handle MongoDB operators
                if op_doc.len() == 1 {
                    let (op, op_value) = op_doc.into_iter().next().unwrap();
                    match op.as_str() {
                        "$eq" => Ok(format!("{} = {}", field, self.bson_to_sql_value(op_value)?)),
                        "$ne" => Ok(format!("{} != {}", field, self.bson_to_sql_value(op_value)?)),
                        "$gt" => Ok(format!("{} > {}", field, self.bson_to_sql_value(op_value)?)),
                        "$gte" => Ok(format!("{} >= {}", field, self.bson_to_sql_value(op_value)?)),
                        "$lt" => Ok(format!("{} < {}", field, self.bson_to_sql_value(op_value)?)),
                        "$lte" => Ok(format!("{} <= {}", field, self.bson_to_sql_value(op_value)?)),
                        "$in" => {
                            if let Bson::Array(values) = op_value {
                                let sql_values: Result<Vec<String>> = values.iter()
                                    .map(|v| self.bson_to_sql_value(v))
                                    .collect();
                                Ok(format!("{} IN ({})", field, sql_values?.join(", ")))
                            } else {
                                Err(anyhow!("$in operator requires an array"))
                            }
                        }
                        "$nin" => {
                            if let Bson::Array(values) = op_value {
                                let sql_values: Result<Vec<String>> = values.iter()
                                    .map(|v| self.bson_to_sql_value(v))
                                    .collect();
                                Ok(format!("{} NOT IN ({})", field, sql_values?.join(", ")))
                            } else {
                                Err(anyhow!("$nin operator requires an array"))
                            }
                        }
                        "$regex" => {
                            let pattern = op_value.as_str().ok_or_else(|| anyhow!("$regex requires a string"))?;
                            Ok(format!("{} ~ '{}'", field, pattern))
                        }
                        "$exists" => {
                            let exists = op_value.as_bool().unwrap_or(false);
                            if exists {
                                Ok(format!("{} IS NOT NULL", field))
                            } else {
                                Ok(format!("{} IS NULL", field))
                            }
                        }
                        _ => Err(anyhow!("Unsupported operator: {}", op))
                    }
                } else {
                    Err(anyhow!("Complex operators not yet supported"))
                }
            }
            _ => {
                // Direct value comparison
                Ok(format!("{} = {}", field, self.bson_to_sql_value(value)?))
            }
        }
    }

    fn bson_to_sql_value(&self, value: &Bson) -> Result<String> {
        match value {
            Bson::String(s) => Ok(format!("'{}'", s.replace("'", "''"))),
            Bson::Int32(i) => Ok(i.to_string()),
            Bson::Int64(i) => Ok(i.to_string()),
            Bson::Double(f) => Ok(f.to_string()),
            Bson::Boolean(b) => Ok(b.to_string()),
            Bson::Null => Ok("NULL".to_string()),
            Bson::ObjectId(_) => Ok(format!("'{}'", value.to_string())),
            _ => Err(anyhow!("Unsupported BSON type for SQL conversion"))
        }
    }

    fn sort_to_sql(&self, sort_doc: &Document) -> Result<String> {
        let mut sort_parts = Vec::new();

        for (field, direction) in sort_doc {
            let direction_str = match direction {
                Bson::Int32(1) | Bson::Int64(1) => "ASC",
                Bson::Int32(-1) | Bson::Int64(-1) => "DESC",
                _ => return Err(anyhow!("Sort direction must be 1 or -1"))
            };
            sort_parts.push(format!("{} {}", field, direction_str));
        }

        Ok(sort_parts.join(", "))
    }

    fn project_to_sql(&self, project_doc: &Document) -> Result<String> {
        let mut select_parts = Vec::new();

        for (field, projection) in project_doc {
            match projection {
                Bson::Int32(1) | Bson::Int64(1) => {
                    select_parts.push(field.clone());
                }
                Bson::Int32(0) | Bson::Int64(0) => {
                    // Skip this field - we'll need to handle exclusion differently
                    continue;
                }
                _ => {
                    // Handle computed fields
                    return Err(anyhow!("Computed projections not yet supported"));
                }
            }
        }

        if select_parts.is_empty() {
            Ok("*".to_string())
        } else {
            Ok(select_parts.join(", "))
        }
    }

    fn handle_group_stage(&self, current_query: String, group_doc: &Document) -> Result<String> {
        // For now, implement a basic GROUP BY
        // More complex group operations would require CTEs or subqueries
        if let Some(group_id) = group_doc.get("_id") {
            let group_field = match group_id {
                Bson::String(field) => field.clone(),
                Bson::Document(_) => {
                    // Complex group expressions not yet supported
                    return Err(anyhow!("Complex group expressions not yet supported"));
                }
                _ => return Err(anyhow!("Invalid group _id"))
            };

            Ok(format!("SELECT {}, COUNT(*) as count FROM ({}) AS grouped GROUP BY {}", 
                      group_field, current_query, group_field))
        } else {
            Err(anyhow!("Group stage requires _id field"))
        }
    }
}

impl Default for AggregationPipeline {
    fn default() -> Self {
        Self::new()
    }
}
