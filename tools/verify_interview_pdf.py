"""Extract, inspect and render the final PDF for visual quality checks."""
from pathlib import Path
import json
import re
import pymupdf
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
document = pymupdf.open(root / 'output/pdf/groot_interview_bible.pdf')
folder = root / 'tmp/pdfs/interview-review'
folder.mkdir(parents=True, exist_ok=True)
text = '\n'.join(page.get_text() for page in document)
questions = []
for page in document:
    for block in page.get_text('dict')['blocks']:
        for line in block.get('lines', []):
            for span in line['spans']:
                match = re.match(r'^Q(\d{3})\.$', span['text'].strip())
                if match and abs(span['size'] - 11.4) < 0.05:
                    questions.append(match.group(1))
assert questions == [f'{i:03}' for i in range(1, 171)], (len(questions), questions)
assert len(document.get_toc()) == 20
assert all(len(page.get_text().strip()) > 100 for page in document)
assert 'auto conflict resolution handling support' in ' '.join(text.split())
assert '20 cases / 148 assertions / 41 CLI calls' in text
assert not any(x in text for x in ['TODO', 'TBD', 'turn0view', 'Lorem ipsum'])
internal_links = [link for page in document for link in page.get_links()
                  if link['kind'] == pymupdf.LINK_GOTO]
assert len(document[1].get_links()) == 40  # Chapter titles and page numbers.
assert len(internal_links) > 40  # Also includes question cross-references.
assert all(0 <= link['page'] < len(document) for link in internal_links)

overflow = []
for index, page in enumerate(document):
    for block in page.get_text('dict')['blocks']:
        for line in block.get('lines', []):
            for span in line['spans']:
                x0, y0, x1, y1 = span['bbox']
                if x0 < 0 or y0 < 0 or x1 > page.rect.width+1 or y1 > page.rect.height+1:
                    overflow.append({'page': index+1, 'text': span['text']})
    page.get_pixmap(matrix=pymupdf.Matrix(1.2, 1.2), alpha=False).save(folder / f'page-{index+1:02}.png')
assert not overflow, overflow

for start in range(0, len(document), 9):
    sheet = Image.new('RGB', (900, 1320), '#dce4e9')
    draw = ImageDraw.Draw(sheet)
    for offset in range(min(9, len(document)-start)):
        page_number = start+offset+1
        pic = Image.open(folder / f'page-{page_number:02}.png')
        pic.thumbnail((280, 396))
        x, y = 10+(offset%3)*300, 25+(offset//3)*440
        sheet.paste(pic, (x, y))
        draw.text((x, y+400), f'Page {page_number}', fill='#142b40')
    sheet.save(folder / f'contact-{start//9+1:02}.png')

report = {'pages': len(document), 'chapters_and_bookmarks': len(document.get_toc()),
          'answered_questions': len(questions), 'page_boundary_overflows': overflow,
          'valid_internal_links': len(internal_links),
          'rendered_pages': len(document), 'pdf_bytes': Path(document.name).stat().st_size,
          'chapter_pages': document.get_toc()}
(root / 'output/verification/pdf-quality.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
