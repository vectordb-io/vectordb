# REST API

所有数据库操作使用 `POST /v2/vectordb/{category}/{action}` 和 JSON 请求。`dbName` 缺省为 `default`。HTTP 服务默认地址 `http://127.0.0.1:19530`。

成功：`{"code":0,"data":...}`。失败：`{"code":非零整数,"message":"原因"}`。

| HTTP | code | 含义 |
| --- | --- | --- |
| 400 | 1100 | 参数错误 |
| 404 | 100 | 数据库 / 集合 / 分区等不存在 |
| 409 | 1101 | 名称 / 主键冲突、加载状态冲突 |
| 501 | 1102 | 未支持操作或参数 |
| 401 | 1800 | Token 不正确 |
| 413 | 1100 | 请求超出 16 MiB |
| 503 | 65535 | 队列已满，可退避重试 |
| 500 | 65535 | 内部存储 / worker 故障 |

未知参数不会被静默接受。HTTP 状态和错误码并不承诺与 Milvus 完全一致。发生连接断开时，写入可能已经提交；重试前查询主键或使用完整实体 upsert。

## 操作清单

下表参数省略共同可选项 `dbName`。方括号表示可选参数。

| 路径后缀 | 请求字段 | data |
| --- | --- | --- |
| databases/create | dbName | `{}` |
| databases/list | — | 名称数组 |
| databases/describe | dbName | `{dbName}` |
| databases/drop | dbName | `{}`；仅空库，default 禁止删除 |
| collections/create | 见下文 | `{}` |
| collections/list | — | 名称数组 |
| collections/has | collectionName | `{has}` |
| collections/describe | collectionName | `{collectionName,schema,indexes,loadState}` |
| collections/get_stats | collectionName | `{rowCount}` |
| collections/get_load_state | collectionName | `{loadState,loadProgress}` |
| collections/drop | collectionName | `{}`；级联删除数据和别名 |
| collections/truncate | collectionName | `{}`；保留 schema / 分区 / ID 序列 |
| collections/rename | collectionName, newCollectionName | `{}`；保留别名指向 |
| collections/load / release | collectionName | `{}` |
| collections/flush | collectionName | `{}`；同步 WAL checkpoint |
| collections/compact | collectionName | `{state:"Completed"}`；同步执行整库 VACUUM |
| partitions/list | collectionName | 分区名称数组 |
| partitions/create | collectionName, partitionName | `{}` |
| partitions/has | collectionName, partitionName | `{has}` |
| partitions/get_stats | collectionName, partitionName | `{rowCount}` |
| partitions/drop / load / release | collectionName, partitionNames | `{}`；drop 禁止删除 `_default` |
| aliases/create / alter | collectionName, aliasName | `{}` |
| aliases/list | collectionName | 别名数组 |
| aliases/describe | aliasName | `{aliasName,collectionName,dbName}` |
| aliases/drop | aliasName | `{}` |
| indexes/create | collectionName, indexParams | `{}` |
| indexes/list | collectionName | 索引名称数组 |
| indexes/describe | collectionName, indexName | 索引配置数组；派生索引延迟构建 |
| indexes/drop | collectionName, indexName | `{}` |
| entities/insert | collectionName, data, [partitionName] | `{insertCount,insertIds}` |
| entities/upsert | collectionName, data, [partitionName] | `{upsertCount,upsertIds}` |
| entities/get | collectionName, id, [partitionNames,outputFields,limit,offset] | 实体数组 |
| entities/query | collectionName, [filter,partitionNames,outputFields,limit,offset,orderByFields] | 实体数组 |
| entities/delete | collectionName, filter, [partitionNames] | `{deleteCount}` |
| entities/search | collectionName, data, [annsField,filter,partitionNames,outputFields,limit,offset,searchParams,groupingField] | 命中数组 |
| entities/hybrid_search | collectionName, search, rerank, [partitionNames,outputFields,limit,offset] | 命中数组 |

实体操作额外接受 `consistencyLevel:"Strong"`；其他一致性级别报错。只读 HTTP 端点为 `GET /healthz`、`GET /metrics`。

## 创建集合

快速方式：

```json
{"collectionName":"products","dimension":3,"metricType":"COSINE","autoID":false}
```

默认主键 `id` / `Int64`，向量字段 `vector` / `FloatVector`，动态字段开启，FLAT 索引，集合与 `_default` 分区立即可查询。可通过 `primaryFieldName`、`vectorFieldName`、`idType`、`vectorFieldType` 自定义。`VarChar` 最大默认长度 65535 字节。稠密 / 二进制 `dimension` 为正整数，最多 32768；二进制维度为 8 的倍数。SparseFloatVector 不传 dimension。

显式方式：

```json
{
  "collectionName":"products",
  "schema":{
    "autoID":false,
    "enableDynamicField":false,
    "fields":[
      {"fieldName":"id","dataType":"Int64","isPrimary":true},
      {"fieldName":"embedding","dataType":"FloatVector","elementTypeParams":{"dim":3}},
      {"fieldName":"title","dataType":"VarChar","elementTypeParams":{"max_length":200}},
      {"fieldName":"price","dataType":"Double","defaultValue":0},
      {"fieldName":"note","dataType":"VarChar","nullable":true},
      {"fieldName":"metadata","dataType":"JSON"}
    ]
  },
  "indexParams":[
    {"fieldName":"embedding","indexName":"embedding_hnsw","indexType":"HNSW","metricType":"COSINE","params":{"M":16,"efConstruction":128}}
  ]
}
```

只允许一个非空 Int64 / VarChar 主键，autoID 仅支持 Int64。显式 Schema 默认禁用动态字段。支持 Int8 / Int16 / Int32 / Int64 / Float / Double / Bool / VarChar / JSON / Array 和三种向量类型。JSON 为对象或数组；Array 为未限定元素类型的 JSON 数组，区别于 Milvus typed Array。浮点绝对值限制为 `1e30`，不接受 NaN / Infinity；向量字段不可为空。保留 `distance`、`queryIndex` 结果字段名；`id` 只能用作主键。

数值向量示例：`[0.1,0.2,0.3]`。稀疏向量示例：`{"7":0.5,"1024":1.2}`，键是规范化 u32 十进制字符串。二进制向量示例：`[15,255]` 表示 16 维，每个元素为 0..255 的字节。

名称必须为 1..255 字节 ASCII 标识符：字母 / 下划线起始，后面可有数字。

## 写入与主键

`data` 必须是 1..10000 个完整实体对象。单批次事务原子提交；维度错误、重复主键、未知字段等任一错误会回滚全部实体。`insert` 拒绝已存在的主键，`upsert` 为完整替换；缺失必填字段报错，不是 patch。

autoID insert 不允许输入主键，返回生成的 `insertIds`。autoID upsert 必须提供主键，新主键会推进序列防止后续碰撞。不同分区共享集合主键空间；upsert 可移动实体到指定分区，省略分区时使用 `_default`。

## 过滤与普通查询

```json
{
  "collectionName":"products",
  "filter":"price >= 10 and metadata['region'] in ['cn','sg'] and title like 'Rust%'",
  "outputFields":["title","price"],
  "orderByFields":["price:desc","id:asc"],
  "limit":20,
  "offset":0
}
```

过滤支持 `==` / `=` / `!=` / `<` / `<=` / `>` / `>=`、`and` / `or` / `not`（也支持 `&&` / `||` / `!`）、括号、`in` / `not in`、`is null` / `is not null`、`like`（`%` 任意串、`_` 单字符）、`array_contains(field,value)` / `json_contains(field,value)`。后者当前仅匹配 JSON 数组元素。

JSON 路径可写 `metadata.region` 或 `metadata['region']`，不支持包含点号的对象键和数组下标。缺失字段按 NULL 处理；NULL 比较及其 NOT 得到未知值，不能匹配行；用 `is null` 显式匹配。输入最多 8192 字节、512 tokens、32 层括号 / not 嵌套。没有 SQL 插值、函数执行、算术、正则或参数模板。

`outputFields` 缺省返回主键；`["*"]` 返回完整实体。`["count(*)"]` 返回过滤后总数，不受 limit / offset 影响。计数不能与普通字段混合。`get` 的 `id` 可以是单个主键或主键数组。

`orderByFields` 仅支持已声明的标量字段。`limit` 默认 10，范围 1..16384；`offset` 默认 0，最大 1000000。`get` 同样受 limit 限制。没有跨请求游标快照。

## 搜索、索引与度量

```json
{
  "collectionName":"products",
  "annsField":"embedding",
  "data":[[1,0,0]],
  "filter":"price < 100",
  "limit":5,
  "outputFields":["title"],
  "searchParams":{"metricType":"COSINE","params":{"ef":128}}
}
```

`data` 为 1..128 个查询向量。多向量字段集合必须指定 `annsField`。每个结果包含 `id`、`distance` 和输出字段；主键不名为 id 时额外保留原主键字段。多个查询的命中结果展开到同一数组，以 `queryIndex`（从 0 开始）区分。没有命中的查询不会出现占位结果。

| 向量 | 支持度量 | 排名方向 |
| --- | --- | --- |
| FloatVector | COSINE / IP | 越大越相似 |
| FloatVector | L2（平方距离） | 越小越相近 |
| SparseFloatVector | IP | 越大越相似 |
| BinaryVector | HAMMING / JACCARD（距离） | 越小越相近 |

零向量的 COSINE 相似度定义为 0；两个全零二进制向量的 JACCARD 距离定义为 0。请求 metricType 必须与索引一致。

每个向量字段最多一个索引。替换索引先 `indexes/drop` 再 `indexes/create`：

```json
{"collectionName":"products","indexParams":[
  {"fieldName":"embedding","indexName":"embedding_ivf","indexType":"IVF_FLAT","metricType":"L2","params":{"nlist":64}}
]}
```

- FLAT：所有向量类型；无额外参数。
- IVF_FLAT：仅 FloatVector；nlist 默认 64，1..4096；查询 nprobe 默认 8，1..4096。
- HNSW：仅 FloatVector；M 默认 16，2..128；efConstruction 默认 128，至少 M，最多 4096；查询 ef 默认 128，1..65536，内部至少达到 limit + offset。

`searchParams.params` 还接受 `radius` / `range_filter`。距离类要求 `range_filter <= distance < radius`；相似度类要求 `radius < score <= range_filter`。省略 range_filter 表示没有该侧边界。

`groupingField` 以一个标量字段去重，每组返回最好的一个结果，分页在分组后应用。不支持 groupSize。分区或过滤较严格时 ANN 候选可能不足 limit，增大搜索参数或使用 FLAT。

## 混合检索

同一集合中多个向量字段，各子搜索 `data` 数量必须一致：

```json
{
  "collectionName":"documents",
  "search":[
    {"annsField":"dense","data":[[1,0,0]],"limit":20},
    {"annsField":"sparse","data":[{"7":1.0}],"limit":20}
  ],
  "rerank":{"strategy":"rrf","params":{"k":60}},
  "limit":5,
  "outputFields":["title"]
}
```

最多 16 个子搜索。加权融合改为 `{"strategy":"weighted","params":{"weights":[0.7,0.3]}}`，每个权重在 [0,1]，至少一个非零，不强制总和为 1。参数不自动生成 embedding 或 BM25 向量。子搜索用 `params` 直接传入 nprobe / ef / range；可用 `metricType` 显式核对度量。

## 认证与指标

如果配置 `VECTORDB_API_KEY`，数据库 API 和 `/metrics` 要求 `Authorization: Bearer <token>`。`/healthz` 不要求认证，只报告进程存活和版本，不能替代磁盘完整性检查。Prometheus 指标为 `vectordb_requests_total`、`vectordb_errors_total`；进程重启后计数归零。
