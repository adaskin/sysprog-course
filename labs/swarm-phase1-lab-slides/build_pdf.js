#!/usr/bin/env node
/* build_pdf.js — renders the lab briefing slides (en/ or tr/) to PDF.
 *
 *   node build_pdf.js en          ->  swarm-phase1-lab-briefing-en.pdf   (the slides)
 *                                     swarm-phase1-lab-notes-en.pdf      (every slide with its notes below)
 *   node build_pdf.js tr          ->  the same two files in Turkish
 *
 * Needs Node.js with Playwright (`npm i playwright`, then `npx playwright install chromium`) and an
 * internet connection for the Google Fonts. The source is the slide format of the Slides artifact:
 * <lang>/project/deck.json and <lang>/project/slides/*.html (one <section> per slide, notes in <aside>).
 */
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const lang = process.argv[2];
if (!['en', 'tr'].includes(lang)) { console.error('usage: node build_pdf.js en|tr'); process.exit(2); }
const root = __dirname;
const proj = path.join(root, lang, 'project');
const assets = JSON.parse(fs.readFileSync(path.join(root, 'assets.json'), 'utf8'));
const deck = JSON.parse(fs.readFileSync(path.join(proj, 'deck.json'), 'utf8'));

const ICONS = {
  Settings: '<circle cx="12" cy="12" r="3.2"/><circle cx="12" cy="12" r="7"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1L7 17M17 7l2.1-2.1"/>',
  Users: '<circle cx="9" cy="8" r="3.5"/><path d="M2 20c0-4 3-6 7-6s7 2 7 6"/><circle cx="17" cy="9" r="2.5"/><path d="M17 14c3 0 5 1.8 5 5"/>',
  Lightning: '<polygon fill="currentColor" points="13 2 4 14 11 14 10 22 20 9 13 9"/>',
  Play: '<polygon fill="currentColor" points="6 4 20 12 6 20"/>',
};
const ARROW = {
  'arrow-right': '0,16 96,16 96,0 160,32 96,64 96,48 0,48',
  'arrow-left': '160,16 64,16 64,0 0,32 64,64 64,48 160,48',
};
const style = (s) => (s.match(/style="([^"]*)"/) || [, ''])[1];
const prop = (st, p) => (st.match(new RegExp('(?:^|;)\\s*' + p + ':\\s*([^;]+)')) || [, ''])[1].trim();

function convert(html) {
  let notes = '';
  html = html.replace(/<aside>([\s\S]*?)<\/aside>/, (_, n) => { notes = n.trim(); return ''; });
  html = html.replace(/src="(\/_blob\/[^"]+)"/g, (m, id) => {
    const a = assets[id];
    if (!a) throw new Error('no asset for ' + id);
    const f = typeof a === 'string' ? a : a.still || a.file;
    return 'src="' + path.resolve(root, f) + '"';
  });
  html = html.replace(/<x-icon name="(\w+)" style="([^"]*)"><\/x-icon>/g, (m, name, st) =>
    `<svg viewBox="0 0 24 24" style="flex:none;display:block;width:${prop(st, 'width')};height:${prop(st, 'height')};color:${prop(st, 'color')}" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">${ICONS[name]}</svg>`);
  html = html.replace(/<x-shape kind="([\w-]+)" style="([^"]*)"><\/x-shape>/g, (m, kind, st) => {
    if (kind === 'ellipse') return `<div style="flex:none;${st};border-radius:50%"></div>`;
    return `<svg viewBox="0 0 160 64" preserveAspectRatio="none" style="flex:none;display:block;width:${prop(st, 'width')};height:${prop(st, 'height')}"><polygon fill="${prop(st, 'background')}" points="${ARROW[kind]}"/></svg>`;
  });
  return { html, notes };
}

const faces = Object.values(deck.faces).map((f) => `<link rel="stylesheet" href="${f.href}">`).join('\n');
const CSS = `
*{box-sizing:border-box}
html,body{margin:0;padding:0;background:#fff}
h1,h2,h3,p,ul,ol{margin:0}
h1{font-size:96px;font-weight:600;line-height:1.1} h2{font-size:64px;font-weight:600;line-height:1.15}
h3{font-size:44px;font-weight:600;line-height:1.2} p{font-size:32px;line-height:1.4}
ul,ol{padding-left:1.1em} li{margin:0 0 .3em}
b{font-weight:600}
table{border-collapse:collapse;width:100%;table-layout:fixed}
th,td{padding:.35em .6em;border-bottom:1px solid rgba(20,25,58,.2);vertical-align:top;text-align:left}
th{font-weight:600}
section{width:1920px;height:1080px;position:relative;overflow:hidden;flex:none}
.page{break-after:page;page-break-after:always}
.notes{width:1920px;height:720px;background:#F3F0E8;color:#14193A;font-family:Inter,Arial,sans-serif;padding:56px 128px;overflow:hidden}
.notes h4{margin:0 0 20px;font-family:'Space Grotesk',Arial,sans-serif;font-size:36px;font-weight:600;color:#2748C9;letter-spacing:3px;text-transform:uppercase}
.notes p{font-size:31px;line-height:1.5}
`;

async function render(withNotes) {
  const pages = deck.order.map((id) => {
    const { html, notes } = convert(fs.readFileSync(path.join(proj, 'slides', id + '.html'), 'utf8'));
    const label = lang === 'tr' ? 'Notlar' : 'Notes';
    return `<div class="page">${html}${withNotes ? `<div class="notes"><h4>${label}</h4><p>${notes}</p></div>` : ''}</div>`;
  });
  const h = withNotes ? 1800 : 1080;
  const doc = `<!doctype html><html lang="${lang}"><head><meta charset="utf-8">${faces}<style>@page{size:1920px ${h}px;margin:0}${CSS}</style></head><body>${pages.join('\n')}</body></html>`;
  const tmp = path.join(root, `.tmp-${lang}-${withNotes ? 'notes' : 'slides'}.html`);
  fs.writeFileSync(tmp, doc);
  const out = path.join(root, `swarm-phase1-lab-${withNotes ? 'notes' : 'briefing'}-${lang}.pdf`);
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
  await page.goto('file://' + tmp, { waitUntil: 'networkidle' });
  await page.evaluate(() => document.fonts.ready);
  await page.pdf({ path: out, width: '1920px', height: h + 'px', printBackground: true, margin: { top: '0', right: '0', bottom: '0', left: '0' } });
  await browser.close();
  fs.unlinkSync(tmp);
  console.log('wrote', path.basename(out));
}

(async () => { await render(false); await render(true); })();
