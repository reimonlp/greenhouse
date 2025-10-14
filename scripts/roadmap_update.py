#!/usr/bin/env python3
"""Auto-update roadmap_reduccion.md summary and section progress bars.

Parses the markdown tables, recomputes per-section completion and rewrites:
- Overall summary block between <!-- AUTO-SUMMARY-START --> and <!-- AUTO-SUMMARY-END -->
- <summary> lines for each <details> section (keeping ordering)

Usage:
  python3 scripts/roadmap_update.py
"""
from __future__ import annotations
import re
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parent.parent
ROADMAP = ROOT / 'docs' / 'roadmap_reduccion.md'

SECTION_HEADER_RE = re.compile(r'^##\s+(\d+)\.\s+([^\n]+)')
DETAILS_SUMMARY_RE = re.compile(r'<summary><strong>(\d+)\.\s+([^<]+)</strong>.*?</summary>')
TABLE_ROW_RE = re.compile(r'^\|\s*([A-Za-z0-9/]+)\s*\|.*?\|\s*([^|]+?)\s*\|')
STATUS_CLEAN_RE = re.compile(r'(✅\s*COMPLETADO|⏳\s*PENDIENTE|🔄\s*EN_PROGRESO|❌\s*DESCARTADO|COMPLETADO|PENDIENTE|EN_PROGRESO|DESCARTADO)', re.IGNORECASE)

DONE_TOKENS = {
    'COMPLETADO','✅ COMPLETADO','✅COMPLETADO','COMPLETADO ✅'
}

BAR_WIDTH = 20

def parse_sections(lines):
    sections = []
    current = None
    for i,l in enumerate(lines):
        m = SECTION_HEADER_RE.match(l)
        if m:
            if current:
                sections.append(current)
            current = { 'num': m.group(1), 'title': m.group(2).strip(), 'rows': [], 'start': i }
        if current and l.startswith('|') and not l.startswith('| ID ') and '---' not in l:
            # crude heuristic: treat as table row inside section if at least 5 pipes
            if l.count('|') >= 5:
                rm = TABLE_ROW_RE.match(l)
                if rm:
                    status_field = rm.group(2)
                    status_match = STATUS_CLEAN_RE.search(status_field)
                    status_norm = status_match.group(1).upper() if status_match else ''
                    current['rows'].append(status_norm)
    if current:
        sections.append(current)
    return sections

def compute_completion(rows):
    total = len(rows)
    if total == 0:
        return 0,0
    done = sum(1 for r in rows if 'COMPLETADO' in r)
    return done,total

def progress_bar(pct: float) -> str:
    filled = round(pct * BAR_WIDTH)
    if filled > BAR_WIDTH: filled = BAR_WIDTH
    bar = '█'*filled + '-'*(BAR_WIDTH-filled)
    return f"[{bar}]"

def update_section_summaries(text: str, section_map: dict) -> str:
    def repl(match):
        num = match.group(1)
        title = match.group(2).strip()
        if num in section_map:
            sm = section_map[num]
            pct = int(round(sm['pct']*100))
            return f"<summary><strong>{num}. {title}</strong> — {sm['done']}/{sm['total']} ({pct}%)</summary>"
        return match.group(0)
    return re.sub(r'<summary><strong>(\d+)\.\s+([^<]+)</strong>.*?</summary>', repl, text)

def extract_priority_breakdown(lines):
    pri_counts = {'🔥':0,'⚖️':0,'🌱':0}
    for l in lines:
        if l.startswith('|') and '|' in l and l.count('|')>=5:
            if '🔥' in l: pri_counts['🔥'] +=1
            elif '⚖️' in l: pri_counts['⚖️'] +=1
            elif '🌱' in l: pri_counts['🌱'] +=1
    return pri_counts

def main():
    text = ROADMAP.read_text(encoding='utf-8')
    lines = text.splitlines()
    sections = parse_sections(lines)
    overall_done = 0
    overall_total = 0
    section_map = {}
    pri_counts = extract_priority_breakdown(lines)
    for s in sections:
        done,total = compute_completion(s['rows'])
        overall_done += done
        overall_total += total
        pct = (done/total) if total else 0.0
        section_map[s['num']] = {
            'title': s['title'],
            'done': done,
            'total': total,
            'pct': pct,
            'bar': progress_bar(pct)
        }

    overall_pct = (overall_done/overall_total) if overall_total else 0.0
    overall_bar = progress_bar(overall_pct)

    summary_block = [
        '<!-- AUTO-SUMMARY-START -->',
        f'Última actualización automática: {datetime.utcnow().isoformat()}Z',
        f'Total tareas: {overall_total} | Completadas: {overall_done} | Avance: {overall_pct*100:.1f}%',
        f'Progreso global: {overall_bar}',
        f'Prioridades: 🔥 {pri_counts["🔥"]} | ⚖️ {pri_counts["⚖️"]} | 🌱 {pri_counts["🌱"]}',
        '',
        'Resumen por sección:'
    ]
    for num in sorted(section_map, key=lambda n:int(n)):
        sm = section_map[num]
        summary_block.append(
            f"- {num}. {sm['title']}: {sm['done']}/{sm['total']} ({sm['pct']*100:.1f}%) {sm['bar']}"
        )
    summary_block.append('<!-- AUTO-SUMMARY-END -->')

    # Replace existing block or insert after Leyenda Visual
    if '<!-- AUTO-SUMMARY-START -->' in text:
        text = re.sub(r'<!-- AUTO-SUMMARY-START -->(.|\n)*?<!-- AUTO-SUMMARY-END -->', '\n'.join(summary_block), text, flags=re.MULTILINE)
    else:
        insertion_point = text.find('Resumen rápido')
        if insertion_point != -1:
            # insert before Resumen rápido
            parts = text.split('Resumen rápido')
            text = parts[0] + '\n' + '\n'.join(summary_block) + '\nResumen rápido' + parts[1]
        else:
            text = '\n'.join(summary_block) + '\n\n' + text

    # Patch section summaries with live counts
    text = update_section_summaries(text, section_map)

    ROADMAP.write_text(text, encoding='utf-8')
    print('Roadmap actualizado.')

if __name__ == '__main__':
    main()
