# vectordb

### Site: 
[vectordb.io](http://vectordb.io)

Vectordb is a database software used in AI service, supporting operations on vectors, such as getKNN (get K Nearest Neighbors). It can work together with the LLM(Large-Language Model), especially in RAG(Retrieval-Augmented Generation) service. It is based on the project([vraft](https://github.com/vectordb-io/vraft)), which supports its distributed architecture.


### vectordb commands:

#### create table
```
(vectordb-cli)> create table --name=test-table --partition_num=10 --replica_num=3 --dim=10
ok
```

#### show tables
```
(vectordb-cli)> show tables;
[
  {
    "size": 1
  },
  [
    "test-table"
  ]
]
```

#### desc table test-table
```
(vectordb-cli)> desc table test-table;
{
  "table": {
    "dim": 10,
    "name": "test-table",
    "partition_names": [
      "test-table#0",
      "test-table#1",
      "test-table#2",
      "test-table#3",
      "test-table#4",
      "test-table#5",
      "test-table#6",
      "test-table#7",
      "test-table#8",
      "test-table#9"
    ],
    "partition_num": 10,
    "path": "/tmp/local_console/data/test-table",
    "replica_num": 3,
    "uid": 0
  }
}
```

#### show partitions
```
(vectordb-cli)> show partitions;
[
  {
    "size": 10
  },
  [
    "test-table#0",
    "test-table#1",
    "test-table#2",
    "test-table#3",
    "test-table#4",
    "test-table#5",
    "test-table#6",
    "test-table#7",
    "test-table#8",
    "test-table#9"
  ]
]
```

#### desc partition
```
(vectordb-cli)> desc partition test-table#0;
{
  "partition": {
    "dim": 10,
    "id": 0,
    "name": "test-table#0",
    "path": "/tmp/local_console/data/test-table/test-table#0",
    "replica_ids": [
      0,
      1,
      2
    ],
    "replica_names": [
      "test-table#0#0",
      "test-table#0#1",
      "test-table#0#2"
    ],
    "replica_num": 3,
    "table_name": "test-table",
    "table_uid": 0,
    "uid": 0
  }
}
```

#### show replicas
```
(vectordb-cli)> show replicas;
[
  {
    "size": 30
  },
  [
    "test-table#0#0",
    "test-table#0#1",
    "test-table#0#2",
    "test-table#1#0",
    "test-table#1#1",
    "test-table#1#2",
    "test-table#2#0",
    "test-table#2#1",
    "test-table#2#2",
    "test-table#3#0",
    "test-table#3#1",
    "test-table#3#2",
    "test-table#4#0",
    "test-table#4#1",
    "test-table#4#2",
    "test-table#5#0",
    "test-table#5#1",
    "test-table#5#2",
    "test-table#6#0",
    "test-table#6#1",
    "test-table#6#2",
    "test-table#7#0",
    "test-table#7#1",
    "test-table#7#2",
    "test-table#8#0",
    "test-table#8#1",
    "test-table#8#2",
    "test-table#9#0",
    "test-table#9#1",
    "test-table#9#2"
  ]
]
```

#### desc replica
```
(vectordb-cli)> desc replica test-table#0#0;
{
  "replica": {
    "dim": 10,
    "id": 0,
    "name": "test-table#0#0",
    "partition_name": "test-table#0",
    "partition_uid": 1,
    "path": "/tmp/local_console/data/test-table/test-table#0/test-table#0#0",
    "table_name": "test-table",
    "table_uid": 0,
    "uid": 0
  }
}
```

#### load data
```
(vectordb-cli)> load --table=test-table --file=/tmp/vec.txt
load line 1: key_0; 0.164831, 0.445726, 0.097244, 0.084204, 0.938371, 0.993784, 0.154138, 0.355323, 0.712672, 0.106347; attach_value_0
load line 2: key_1; 0.644472, 0.876031, 0.787079, 0.434390, 0.711722, 0.874121, 0.902163, 0.195676, 0.613694, 0.603673; attach_value_1
load line 3: key_2; 0.616118, 0.992739, 0.923674, 0.620808, 0.059192, 0.901620, 0.298015, 0.445837, 0.292800, 0.094566; attach_value_2
load line 4: key_3; 0.776293, 0.187398, 0.862613, 0.587328, 0.585514, 0.009252, 0.565339, 0.847481, 0.628480, 0.766569; attach_value_3
loading ...
load line 998: key_997; 0.206405, 0.623443, 0.877383, 0.178855, 0.404137, 0.395500, 0.128035, 0.048595, 0.566553, 0.578899; attach_value_997
load line 999: key_998; 0.298418, 0.379632, 0.866918, 0.528748, 0.472486, 0.168596, 0.927100, 0.348894, 0.966354, 0.132273; attach_value_998
load line 1000: key_999; 0.518561, 0.439811, 0.279292, 0.922664, 0.023825, 0.566798, 0.716824, 0.992219, 0.369687, 0.625317; attach_value_999
ok
```

#### build index
```
(vectordb-cli)> build index --table=test-table --annoy_tree_num=10
ok
```

#### desc engine
```
(vectordb-cli)> desc engine test-table#0#0;
{
  "vengine": {
    "indices": [
      {
        "index": {
          "annoy_path_file": "/tmp/local_console/data/test-table/test-table#0/test-table#0#0/index/IndexAnnoy_1719590965654646882/annoy.tree",
          "keyid_path": "/tmp/local_console/data/test-table/test-table#0/test-table#0#0/index/IndexAnnoy_1719590965654646882/keyid",
          "meta": {
            "param": {
              "annoy_tree_num": 10,
              "dim": 10,
              "distance_type": [
                0,
                "Cosine"
              ],
              "index_type": [
                0,
                "IndexAnnoy"
              ],
              "path": "/tmp/local_console/data/test-table/test-table#0/test-table#0#0/index/IndexAnnoy_1719590965654646882",
              "timestamp": [
                1719590965654646882,
                "2024:06:29 00:09:25 654646882"
              ]
            }
          },
          "meta_path_": "/tmp/local_console/data/test-table/test-table#0/test-table#0#0/index/IndexAnnoy_1719590965654646882/meta"
        },
        "time": "2024:06:29 00:09:25 654646882"
      }
    ],
    "meta": {
      "dim": 10
    }
  }
}
```

#### getknn by key
```
(vectordb-cli)> getknn --table=test-table --key=key_0 --limit=20
{distance:0.000000, key:key_0, attach_value:attach_value_0}
{distance:0.416249, key:key_493, attach_value:attach_value_493}
{distance:0.469023, key:key_830, attach_value:attach_value_830}
{distance:0.551433, key:key_101, attach_value:attach_value_101}
{distance:0.574484, key:key_135, attach_value:attach_value_135}
{distance:0.586908, key:key_521, attach_value:attach_value_521}
{distance:0.589036, key:key_340, attach_value:attach_value_340}
{distance:0.601359, key:key_554, attach_value:attach_value_554}
{distance:0.623582, key:key_328, attach_value:attach_value_328}
{distance:0.626737, key:key_954, attach_value:attach_value_954}
{distance:0.637985, key:key_975, attach_value:attach_value_975}
{distance:0.649394, key:key_485, attach_value:attach_value_485}
{distance:0.666963, key:key_1, attach_value:attach_value_1}
{distance:0.672188, key:key_487, attach_value:attach_value_487}
{distance:0.690556, key:key_955, attach_value:attach_value_955}
{distance:0.691762, key:key_846, attach_value:attach_value_846}
{distance:0.697253, key:key_72, attach_value:attach_value_72}
{distance:0.699238, key:key_805, attach_value:attach_value_805}
{distance:0.706862, key:key_177, attach_value:attach_value_177}
{distance:0.707307, key:key_900, attach_value:attach_value_900}
```

#### getknn by vector
```
(vectordb-cli)> getknn --table=test-table --vec=0.435852,0.031869,0.161108,0.055670,0.846847,0.604385,0.075282,0.386435,0.407000,0.101307 --limit=20
{distance:0.375453, key:key_753, attach_value:attach_value_753}
{distance:0.379140, key:key_468, attach_value:attach_value_468}
{distance:0.410338, key:key_0, attach_value:attach_value_0}
{distance:0.422657, key:key_589, attach_value:attach_value_589}
{distance:0.431398, key:key_267, attach_value:attach_value_267}
{distance:0.442684, key:key_312, attach_value:attach_value_312}
{distance:0.449014, key:key_278, attach_value:attach_value_278}
{distance:0.454012, key:key_885, attach_value:attach_value_885}
{distance:0.455855, key:key_766, attach_value:attach_value_766}
{distance:0.467043, key:key_824, attach_value:attach_value_824}
{distance:0.469801, key:key_77, attach_value:attach_value_77}
{distance:0.472218, key:key_507, attach_value:attach_value_507}
{distance:0.473036, key:key_521, attach_value:attach_value_521}
{distance:0.473579, key:key_84, attach_value:attach_value_84}
{distance:0.479299, key:key_20, attach_value:attach_value_20}
{distance:0.483883, key:key_117, attach_value:attach_value_117}
{distance:0.488278, key:key_799, attach_value:attach_value_799}
{distance:0.489877, key:key_738, attach_value:attach_value_738}
{distance:0.495219, key:key_787, attach_value:attach_value_787}
{distance:0.499322, key:key_15, attach_value:attach_value_15}
```

#### put vector
```
(vectordb-cli)> put --table=test-table --key=kkk --vec=0.435852,0.031869,0.161108,0.055670,0.846847,0.604385,0.075282,0.386435,0.407000,0.101307 --attach_value=aaavvv
ok
```

#### get vector
```
(vectordb-cli)> get --table=test-table --key=kkk  
{
  "vec-obj": {
    "key": "kkk",
    "value": {
      "attach_value": "aaavvv",
      "vec": [
        0.4358519911766052,
        0.03186900168657303,
        0.16110800206661224,
        0.05567000061273575,
        0.8468469977378845,
        0.6043850183486938,
        0.07528200000524521,
        0.38643500208854675,
        0.40700000524520874,
        0.10130699723958969
      ]
    }
  }
}
```

#### delete vector
```
(vectordb-cli)> del --table=test-table --key=kkk  
ok
```

#### help
```
(vectordb-cli)> help
example:

help
version
quit
meta
put --table=test-table --key=key_0 --vec=0.435852,0.031869,0.161108,0.055670,0.846847,0.604385,0.075282,0.386435,0.407000,0.101307 --attach_value=aaavvv
get --table=test-table --key=key_0
del --table=test-table --key=key_0
getknn --table=test-table --key=key_0 --limit=20
getknn --table=test-table --vec=0.435852,0.031869,0.161108,0.055670,0.846847,0.604385,0.075282,0.386435,0.407000,0.101307 --limit=20
load --table=test-table --file=/tmp/vec.txt
create table --name=test-table --partition_num=10 --replica_num=3 --dim=10
build index --table=test-table --annoy_tree_num=10
desc table test-table
desc partition test-table#0
desc replica test-table#0#0
desc engine test-table#0#0
show tables
show partitions
show replicas
```

