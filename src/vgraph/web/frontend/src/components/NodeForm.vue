<template>
  <div class="node-form">
    <h3>添加节点</h3>
    <el-form :model="form" label-width="80px">
      <el-form-item label="节点ID">
        <el-input-number v-model="form.id" :min="1" />
      </el-form-item>
      <el-form-item label="节点名称">
        <el-input v-model="form.name" placeholder="请输入节点名称" />
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="handleSubmit">添加节点</el-button>
      </el-form-item>
    </el-form>
  </div>
</template>

<script>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import { addNode } from '../api/graph'

export default {
  emits: ['node-added'],
  setup(props, { emit }) {
    const form = ref({
      id: 1,
      name: ''
    })

    const handleSubmit = async () => {
      try {
        if (!form.value.name) {
          ElMessage.warning('请输入节点名称')
          return
        }

        await addNode({
          id: form.value.id,
          properties: {
            name: form.value.name
          }
        })

        ElMessage.success('节点添加成功')
        emit('node-added')
        form.value.id++
        form.value.name = ''
      } catch (error) {
        console.error('添加节点失败:', error)
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
.node-form {
  margin-bottom: 20px;
}
</style> 