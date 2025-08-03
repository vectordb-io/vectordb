<template>
  <div class="edge-form">
    <h3>添加边</h3>
    <el-form :model="form" label-width="80px">
      <el-form-item label="起始节点">
        <el-input-number v-model="form.from" :min="1" />
      </el-form-item>
      <el-form-item label="目标节点">
        <el-input-number v-model="form.to" :min="1" />
      </el-form-item>
      <el-form-item label="权重">
        <el-input-number v-model="form.weight" :min="0" :precision="1" :step="0.1" />
      </el-form-item>
      <el-form-item label="边类型">
        <el-input v-model="form.type" placeholder="请输入边的类型" />
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="handleSubmit">添加边</el-button>
      </el-form-item>
    </el-form>
  </div>
</template>

<script>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import { addEdge } from '../api/graph'

export default {
  emits: ['edge-added'],
  setup(props, { emit }) {
    const form = ref({
      from: 1,
      to: 2,
      weight: 1.0,
      type: '连接'
    })

    const handleSubmit = async () => {
      try {
        if (!form.value.type) {
          ElMessage.warning('请输入边的类型')
          return
        }

        await addEdge({
          from: form.value.from,
          to: form.value.to,
          weight: form.value.weight,
          properties: {
            type: form.value.type
          }
        })

        ElMessage.success('边添加成功')
        emit('edge-added')
      } catch (error) {
        console.error('添加边失败:', error)
      }
    }

    return {
      form,
      handleSubmit
    }
  }
}
</script>

<style scoped>
.edge-form {
  margin-bottom: 20px;
}
</style> 