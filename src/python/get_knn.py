import sys
import os

# 将 vdb_module.so 的目录添加到 sys.path
sys.path.append('/home/mhlee/Work/dev/vraft/output/libs')

import vdb_module

from langchain.embeddings import HuggingFaceEmbeddings
from langchain.text_splitter import RecursiveCharacterTextSplitter
from transformers import AutoTokenizer, AutoModel
import torch
import os


# 加载本地模型
model_name = "/home/mhlee/Work/dev/vraft/src/python/all-MiniLM-L6-v2"
tokenizer = AutoTokenizer.from_pretrained(model_name)
model = AutoModel.from_pretrained(model_name)

def embed_text(text):
    inputs = tokenizer(text, return_tensors="pt", truncation=True, padding=True)
    with torch.no_grad():
        outputs = model(**inputs)
    return outputs.last_hidden_state.mean(dim=1).squeeze().numpy()



# 创建 VdbEngine 实例
engine = vdb_module.VdbEngine("/tmp/local_console")

table = "test-table"
#vec = [0.172457, 0.383009, 0.255386, 0.016210, 0.705780, 0.920516, 0.678624, 0.796226, 0.115947, 0.185988]  # 示例向量
vec = embed_text("TDengine一个cpu可以处理几个vnode？")
limit = 5

res_code, results = engine.get_knn(table, vec, limit)

print("Response code:", res_code)
for result in results:
    print(result.to_print_string())
