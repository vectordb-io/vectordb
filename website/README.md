# vectordb.io 官网

根据 Rust 实现重新制作的静态单页：项目介绍、浏览器内二维向量搜索示意、可复制启动示例、功能边界及文档链接。无构建依赖、外部字体、追踪脚本或第三方 CDN。

## 预览与检查

从仓库根目录运行：

```sh
python3 -m http.server 4173 --bind 127.0.0.1 --directory website
python3 scripts/check_website.py
```

访问 http://127.0.0.1:4173/ 。验证桌面 / 手机布局、度量切换、Top K、查询位置、代码步骤切换与复制。交互图完全在浏览器计算，明确标注为示意，不会连接真实数据库。

## 域名与发布

保留原有 `.github/workflows/pages.yml`、`website/CNAME`（内容 `vectordb.io`）和 `.nojekyll`。工作流仅上传 `website/`，不依赖 Rust 编译。推送 main 的网站或 Pages 工作流变化会发布，亦支持手工 workflow_dispatch。

在原仓库 `vectordb-io/vectordb` 的 Settings → Pages 中保持：

- Source = GitHub Actions。
- Custom domain = vectordb.io。
- Enforce HTTPS 按已生效配置保留。

**Actions 发布模式下 CNAME 文件本身不能建立域名绑定，真正生效的是 Pages 设置。** 保留现有有效 DNS 记录即可，无须因 Rust 重构而改变域名解析。本文不把尚未核对的线上设置当作已确认事实。

发布后应验证 `https://vectordb.io/` 的新页面、HTTPS、静态资源和 GitHub 文档链接。旧站路径进入 `404.html`，引导访问新官网与文档；未实现所有旧 URL 的逐一重定向。

发布只会在代码实际提交并推送后发生，本地预览不等于线上更新。

官方说明：[自定义域名](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/managing-a-custom-domain-for-your-github-pages-site)。
