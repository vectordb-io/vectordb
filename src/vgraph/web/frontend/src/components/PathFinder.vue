<template>
  <div class="path-finder">
    <h3>查找最短路径</h3>
    <el-form :model="form" label-width="80px">
      <el-form-item label="起始节点">
        <el-input-number v-model="form.from" :min="1" />
      </el-form-item>
      <el-form-item label="目标节点">
        <el-input-number v-model="form.to" :min="1" />
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="handleFind">查找路径</el-button>
      </el-form-item>
    </el-form>
  </div>
</template>

<script>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import { findShortestPath } from '../api/graph'

export default {
  emits: ['path-found'],
  setup(props, { emit }) {
    const form = ref({
      from: 1,
      to: 2
    })

    const handleFind = async () => {
      try {
        const result = await findShortestPath(form.value.from, form.value.to)
        emit('path-found', result.nodes)
        ElMessage.success(`找到路径，总权重: ${result.total_weight}`)
      } catch (error) {
        console.error('查找路径失败:', error)
        ElMessage.error('未找到路径')
      }
    }

    return {
      form,
      handleFind
    }
  }
}
</script>

<style scoped>
.path-finder {
  margin-bottom: 20px;
}
</style> 