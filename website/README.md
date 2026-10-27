# vectordb.io 官网

根据 Rust 实现重新制作的静态单页：项目介绍、浏览器内二维向量搜索示意、可复制启动示例、功能边界及文档链接。支持 English、中文、日本語，首次访问默认英文，不根据浏览器语言自动切换。导航栏语言选择会更新整页及交互文案，并通过 localStorage 记住选择；存储不可用时仍可正常切换。404 页面使用同一套语言选择。无构建依赖、外部字体、追踪脚本或第三方 CDN。

## 预览与检查

从仓库根目录运行：

```sh
python3 -m http.server 4173 --bind 127.0.0.1 --directory website
python3 scripts/check_website.py
```

访问 http://127.0.0.1:4173/ 。验证三种语言的桌面 / 手机布局、刷新后的语言记忆、度量切换、Top K、查询位置、代码步骤切换与复制。交互图完全在浏览器计算，明确标注为示意，不会连接真实数据库。

## 多语言维护

`i18n.js` 集中维护 `en`、`zh-CN`、`ja` 文案。HTML 的 `data-i18n` 标记对应文案键，`data-i18n-aria-label` 和 `data-i18n-content` 用于无障碍标签和页面元信息。HTML 本身保留英文内容作为脚本加载前的默认展示。动态检索与代码示例通过同一字典获取文案，切换语言不会重置查询参数或当前代码步骤。代码中的 API 路径、字段名和命令保持一致，只翻译说明与注释。

文案仅来自本地字典；含 HTML 的文案只用于既有的换行、强调色和装饰元素，不应接入不可信内容。外链文档保持原有地址，不代表仓库文档已全部翻译。

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
