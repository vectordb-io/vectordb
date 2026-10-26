(() => {
  'use strict';
  let language = 'zh';
  let queryIndex = 0;
  const queries = [{ x: 210, y: 152 }, { x: 350, y: 215 }, { x: 390, y: 95 }];
  // Deterministic sample data. This diagram is independent of the database.
  const points = Array.from({ length: 62 }, (_, i) => ({
    x: 55 + ((i * 137 + 29) % 425),
    y: 40 + ((i * 97 + i * i * 7) % 255),
    id: i + 1
  }));
  const group = document.getElementById('map-content');
  const neighbors = document.getElementById('neighbors');
  const svgNS = 'http://www.w3.org/2000/svg';
  function element(name, attributes, text) {
    const node = document.createElementNS(svgNS, name);
    for (const [key, value] of Object.entries(attributes)) node.setAttribute(key, value);
    if (text) node.textContent = text;
    group.appendChild(node);
    return node;
  }
  function renderMap() {
    const query = queries[queryIndex];
    const count = Number(neighbors.value);
    const nearest = points.map(point => ({ ...point, distance: Math.hypot(point.x - query.x, point.y - query.y) }))
      .sort((a, b) => a.distance - b.distance).slice(0, count);
    const selected = new Set(nearest.map(point => point.id));
    group.replaceChildren();
    element('circle', { cx: query.x, cy: query.y, r: nearest[count - 1].distance + 9, fill: '#46634a08', stroke: '#9bac91', 'stroke-dasharray': '4 5', 'stroke-width': 1 });
    for (const point of nearest) element('line', { x1: query.x, y1: query.y, x2: point.x, y2: point.y, stroke: '#8d9e83', 'stroke-width': 1 });
    for (const point of points) {
      element('circle', { cx: point.x, cy: point.y, r: selected.has(point.id) ? 4.5 : 2.8, fill: selected.has(point.id) ? '#46634a' : '#a7b09b', opacity: selected.has(point.id) ? 1 : .64 });
      if (selected.has(point.id)) element('text', { x: point.x + 8, y: point.y - 7, class: 'point-label' }, `v${String(point.id).padStart(2, '0')}`);
    }
    element('circle', { cx: query.x, cy: query.y, r: 13, fill: '#d74d2d14', stroke: '#d74d2d', 'stroke-width': .6 });
    element('circle', { cx: query.x, cy: query.y, r: 5, fill: '#d74d2d', stroke: '#f5f4ef', 'stroke-width': 2 });
    element('text', { x: query.x + 17, y: query.y + 22, class: 'query-label' }, `QUERY ${'ABC'[queryIndex]}`);
    document.getElementById('result-label').textContent = language === 'zh' ? `已找到 ${count} 个近邻` : `${count} nearest neighbors found`;
  }
  document.querySelectorAll('[data-query]').forEach(button => button.addEventListener('click', () => {
    queryIndex = Number(button.dataset.query);
    document.querySelectorAll('[data-query]').forEach(item => item.setAttribute('aria-pressed', String(item === button)));
    renderMap();
  }));
  neighbors.addEventListener('change', renderMap);
  const languageButton = document.getElementById('language');
  function setLanguage(nextLanguage) {
    language = nextLanguage === 'en' ? 'en' : 'zh';
    document.documentElement.lang = language === 'zh' ? 'zh-CN' : 'en';
    document.querySelectorAll('[data-zh][data-en]').forEach(node => {
      // Translation strings are static, authored in this page, never user input.
      node.innerHTML = node.dataset[language];
    });
    languageButton.textContent = language === 'zh' ? 'EN' : '中文';
    languageButton.setAttribute('aria-label', language === 'zh' ? 'Switch to English' : '切换到中文');
    document.getElementById('map-title').textContent = language === 'zh' ? '二维向量近邻示意' : 'Two-dimensional nearest-neighbor illustration';
    document.getElementById('map-description').textContent = language === 'zh' ? '选择查询点，查看二维空间中距离最近的示例向量。' : 'Choose a query point to see the closest sample vectors in two dimensions.';
    const description = language === 'zh' ? 'VectorDB：探索向量检索的底层实现。基于 C++、HNSW 与 RocksDB，阅读源码、了解架构并开始构建。' : 'Explore vector search from the inside out. Read the C++ source, understand HNSW and RocksDB, and start building VectorDB.';
    document.querySelector('meta[name="description"]').content = description;
    renderMap();
    try { localStorage.setItem('vectordb-language', language); } catch (_) { /* Storage may be disabled. */ }
  }
  languageButton.addEventListener('click', () => setLanguage(language === 'zh' ? 'en' : 'zh'));
  let savedLanguage;
  try { savedLanguage = localStorage.getItem('vectordb-language'); } catch (_) { /* Use default language. */ }
  setLanguage(savedLanguage || 'zh');
  const copyButton = document.getElementById('copy-code');
  let copyTimer;
  copyButton.addEventListener('click', async () => {
    const code = document.getElementById('build-code');
    const status = document.getElementById('copy-status');
    clearTimeout(copyTimer);
    try {
      await navigator.clipboard.writeText(code.textContent);
      const message = language === 'zh' ? '已复制' : 'Copied';
      copyButton.textContent = message;
      status.textContent = message;
    } catch (_) {
      const selection = window.getSelection();
      const range = document.createRange();
      range.selectNodeContents(code);
      selection.removeAllRanges();
      selection.addRange(range);
      status.textContent = language === 'zh' ? '请按 Ctrl+C 或 Command+C 复制已选中的命令。' : 'Press Ctrl+C or Command+C to copy the selected commands.';
      copyButton.textContent = language === 'zh' ? '请手动复制' : 'Copy selection';
    }
    copyTimer = setTimeout(() => { copyButton.textContent = copyButton.dataset[language]; }, 2400);
  });
})();
