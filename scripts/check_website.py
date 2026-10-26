#!/usr/bin/env python3
"""Verify local website assets, fragment targets, repo links and domain files."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit, unquote
import re
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'website'
class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.refs = []
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            assert attrs['id'] not in self.ids, f'duplicate id {attrs["id"]}'
            self.ids.add(attrs['id'])
        for key in ('src','href'):
            if key in attrs:
                self.refs.append(attrs[key])
for file in SITE.glob('*.html'):
    page = Page()
    page.feed(file.read_text())
    for ref in page.refs:
        url = urlsplit(ref)
        if not url.scheme and not url.netloc:
            if url.path:
                path = (file.parent / unquote(url.path)).resolve()
                assert path == SITE or SITE in path.parents, f'outside site: {ref}'
                assert path.exists(), f'missing asset {ref}'
            elif url.fragment:
                assert url.fragment in page.ids, f'missing anchor {ref}'
        elif url.netloc == 'github.com' and '/blob/main/' in url.path:
            relative = url.path.split('/blob/main/',1)[1]
            assert (ROOT / relative).is_file(), f'missing repository file {relative}'
assert (SITE / 'CNAME').read_text().strip() == 'vectordb.io'
assert (SITE / '.nojekyll').is_file()
assert 'path: website' in (ROOT / '.github/workflows/pages.yml').read_text()
for file in [ROOT/'README.md', *ROOT.glob('docs/*.md')]:
    for target in re.findall(r'\]\(([^)]+)\)',file.read_text()):
        if not urlsplit(target).scheme and not target.startswith('#'):
            assert (file.parent / target.split('#')[0]).exists(), f'{file.name}: missing {target}'
subprocess.run(['node','--check',str(SITE/'app.js')],check=True)
print('PASS: website assets, anchors, repository documentation links, domain and Pages deployment path')
