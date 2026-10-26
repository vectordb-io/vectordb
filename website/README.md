# VectorDB website

无构建依赖的静态官网：HTML、CSS、JavaScript，默认中文，可切换英文。
所有页面资源均为本地文件，不依赖外部字体、CDN 或追踪脚本。
检索图是浏览器内二维欧氏距离计算的交互示意，不连接数据库、不代表性能测试。

## 本地预览

在仓库根目录运行：

```sh
python3 -m http.server 8000 --directory website --bind 127.0.0.1
```

打开 http://127.0.0.1:8000 。支持直接打开 `index.html`；复制功能受浏览器安全上下文限制，失败时会选中代码供手动复制。

## 首次启用 GitHub Pages

1. 打开 https://github.com/vectordb-io/vectordb/settings/pages 。
2. 在 **Build and deployment → Source** 中选择 **GitHub Actions**。
3. 在 **Actions → Deploy website to GitHub Pages** 中执行 **Run workflow**。后续推送到 `main` 的 `website/` 或发布工作流变更会自动发布。
4. 部署成功后可先通过 https://vectordb-io.github.io/vectordb/ 检查页面。
5. 在 Pages 设置的 **Custom domain** 填写 `vectordb.io` 并保存。使用 Actions 发布时，必须在这里设置域名；`website/CNAME` 仅记录目标域名，不会代替该设置。
6. 在 DNS 服务商后台配置下表。先确认现有域名服务可以切换：2026-10-26 检查时，根域名指向 `8.219.145.122`；README 曾引用 `/coverage/report/index.html`，此静态站点不含该报告，切换前应另行迁移或安排子域名。
7. 等待 GitHub DNS 检查与 HTTPS 证书签发生效，启用 **Enforce HTTPS**。DNS 传播可能需要 24 小时。

| 类型 | 主机记录 | 值 |
| --- | --- | --- |
| A | @ | 185.199.108.153 |
| A | @ | 185.199.109.153 |
| A | @ | 185.199.110.153 |
| A | @ | 185.199.111.153 |
| CNAME | www | vectordb-io.github.io |

根域名旧的 A/AAAA 或冲突记录需要协调替换；保留无关的 MX/TXT 邮箱与验证记录。`www` 的 CNAME 不带仓库路径。

官方说明：
- https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages
- https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/managing-a-custom-domain-for-your-github-pages-site

## 编辑内容

- `index.html`：页面结构、项目文案和构建命令。双语文案由元素的 `data-zh` / `data-en` 属性提供。
- `styles.css`：响应式布局及视觉样式。
- `app.js`：语言切换、近邻示意与复制命令。
- `favicon.svg`：站点图标。
- `../.github/workflows/pages.yml`：仅上传 `website/`，不会上传数据库源码或触发数据库编译。

更新后检查桌面与手机布局、语言切换、查询点 / Top K、复制按钮以及页面内外链接。
