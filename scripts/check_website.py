#!/usr/bin/env python3
"""Verify local website assets, fragment targets, repo links and domain files."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit, unquote
import re
import json
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'website'
translations = json.JSONDecoder().raw_decode(
    (SITE / 'i18n.js').read_text().split('const messages = ', 1)[1]
)[0]
assert set(translations) == {'en', 'zh-CN', 'ja', 'es', 'pt-BR'}
for language, messages in translations.items():
    assert messages.keys() == translations['en'].keys(), f'incomplete locale: {language}'
    assert all(isinstance(value, str) and value.strip() for value in messages.values())
class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.refs = []
        self.languages = []
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'html':
            assert attrs.get('lang') == 'en', 'static pages must default to English'
        if tag == 'option' and 'lang' in attrs:
            assert attrs.get('value') == attrs['lang'], 'language option mismatch'
            self.languages.append(attrs['lang'])
        for attribute, key in attrs.items():
            if attribute.startswith('data-i18n'):
                assert key in translations['en'], f'missing translation: {key}'
        if 'id' in attrs:
            assert attrs['id'] not in self.ids, f'duplicate id {attrs["id"]}'
            self.ids.add(attrs['id'])
        for key in ('src','href'):
            if key in attrs:
                self.refs.append(attrs[key])
for file in SITE.glob('*.html'):
    page = Page()
    page.feed(file.read_text())
    assert sorted(page.languages) == sorted(translations), f'{file.name}: incomplete language picker'
    for ref in page.refs:
        url = urlsplit(ref)
        if not url.scheme and not url.netloc:
            if url.path:
                path = ((SITE / unquote(url.path).lstrip('/')) if url.path.startswith('/') else (file.parent / unquote(url.path))).resolve()
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
for file in [*ROOT.glob('README*.md'), *ROOT.glob('docs/*.md')]:
    for target in re.findall(r'\]\(([^)]+)\)',file.read_text()):
        if not urlsplit(target).scheme and not target.startswith('#'):
            assert (file.parent / target.split('#')[0]).exists(), f'{file.name}: missing {target}'
for script in SITE.glob('*.js'):
    subprocess.run(['node','--check',str(script)],check=True)
print('PASS: website assets, anchors, translations, English defaults, repository documentation links, domain and Pages deployment path')
