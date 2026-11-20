"""Build the interview guide with linked contents, bookmarks and print layout."""
from pathlib import Path
import html
import re

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, Frame, PageTemplate, Paragraph, Spacer, PageBreak,
    Preformatted, KeepTogether, Flowable, HRFlowable,
)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'output/pdf/groot_interview_bible.md'
OUTPUT = ROOT / 'output/pdf/groot_interview_bible.pdf'
NAVY = colors.HexColor('#142B40')
TEAL = colors.HexColor('#007E83')
INK = colors.HexColor('#203345')
MUTED = colors.HexColor('#586979')
PALE = colors.HexColor('#F0F5F7')
GOLD = colors.HexColor('#EAB967')

for name, file in [('Body', 'calibri.ttf'), ('Body-Bold', 'calibrib.ttf'),
                   ('Body-Italic', 'calibrii.ttf'), ('Mono', 'consola.ttf')]:
    pdfmetrics.registerFont(TTFont(name, str(Path('C:/Windows/Fonts') / file)))
pdfmetrics.registerFontFamily('Body', normal='Body', bold='Body-Bold', italic='Body-Italic', boldItalic='Body-Bold')

WIDTH, HEIGHT = A4
LEFT, RIGHT = 22*mm, 20*mm
BODY_WIDTH = WIDTH - LEFT - RIGHT
styles = {
    'body': ParagraphStyle('BodyText', fontName='Body', fontSize=10.4, leading=14.4,
                           textColor=INK, spaceAfter=7, allowWidows=0, allowOrphans=0),
    'chapter': ParagraphStyle('Chapter', fontName='Body-Bold', fontSize=25, leading=29,
                              textColor=NAVY, spaceAfter=17, keepWithNext=True),
    'section': ParagraphStyle('Section', fontName='Body-Bold', fontSize=14.2, leading=18,
                              textColor=TEAL, spaceBefore=13, spaceAfter=8, keepWithNext=True),
    'question': ParagraphStyle('Question', fontName='Body-Bold', fontSize=11.4, leading=15.1,
                               textColor=NAVY, spaceBefore=12, spaceAfter=6, keepWithNext=True),
    'bullet': ParagraphStyle('Bullet', fontName='Body', fontSize=10.1, leading=14,
                             textColor=INK, leftIndent=12, firstLineIndent=-10,
                             spaceAfter=6, allowWidows=0, allowOrphans=0),
    'source': ParagraphStyle('Source', fontName='Body-Italic', fontSize=8.6, leading=11.6,
                             textColor=MUTED, spaceBefore=4, spaceAfter=9),
    'code': ParagraphStyle('Code', fontName='Mono', fontSize=7.7, leading=10.5,
                           textColor=INK, backColor=PALE, borderPadding=10,
                           spaceBefore=6, spaceAfter=12),
    'toc': ParagraphStyle('TOC', fontName='Body', fontSize=10.5, leading=14,
                          textColor=INK, leftIndent=0, firstLineIndent=0,
                          spaceBefore=1, spaceAfter=3),
    'reference_body': ParagraphStyle('ReferenceBody', fontName='Body', fontSize=10,
                                     leading=13.4, textColor=INK, spaceAfter=6),
    'reference_bullet': ParagraphStyle('ReferenceBullet', fontName='Body', fontSize=9.5,
                                       leading=12.8, textColor=INK, leftIndent=12,
                                       firstLineIndent=-10, spaceAfter=4),
}

def rich(text):
    text = html.escape(text)
    # Make external source links clickable without changing displayed text.
    text = re.sub(r'https://[^\s<]+', lambda m: '<link href="'+m.group(0)+'" color="#007E83">'+m.group(0)+'</link>', text)
    return re.sub(r'\bQ\d{3}\b', lambda m: '<link href="#'+m.group(0)+'" color="#007E83">'+m.group(0)+'</link>', text)

class Cover(Flowable):
    def __init__(self):
        super().__init__()
        self.width, self.height = BODY_WIDTH, HEIGHT-44*mm

    def draw(self):
        c = self.canv
        x, y = 0, self.height
        c.setFillColor(TEAL)
        c.rect(x, y-12, 52, 5, fill=1, stroke=0)
        c.setFillColor(NAVY)
        c.setFont('Body-Bold', 64)
        c.drawString(x-2, y-103, 'GROOT')
        c.setFont('Body-Bold', 27)
        c.drawString(x, y-150, 'The project')
        c.drawString(x, y-184, 'interview bible')
        c.setFont('Body', 13)
        c.setFillColor(MUTED)
        c.drawString(x, y-222, 'C++20  /  SHA-256  /  Graphs  /  Diff & merge')
        c.setStrokeColor(colors.HexColor('#CBD8DF'))
        c.line(x, y-250, BODY_WIDTH, y-250)
        p = Paragraph('Understand the code. Explain the decisions.<br/>Defend the CV. Handle the follow-ups.',
                      ParagraphStyle('CoverDeck', fontName='Body', fontSize=17, leading=24, textColor=INK))
        p.wrap(BODY_WIDTH-10, 100)
        p.drawOn(c, x, y-329)
        c.setFillColor(PALE)
        c.roundRect(x, y-445, BODY_WIDTH, 76, 7, fill=1, stroke=0)
        for pos, big, small in [(15, '20', 'chapters'), (160, '170', 'answered questions'), (335, '8', 'worked exercises')]:
            c.setFillColor(TEAL)
            c.setFont('Body-Bold', 25)
            c.drawString(x+pos, y-404, big)
            c.setFont('Body', 10)
            c.setFillColor(INK)
            c.drawString(x+pos, y-425, small)
        p = Paragraph('Based on the reviewed working source and your supplied CV entry. Includes a claim audit, code walkthroughs, verified bug fixes, design tradeoffs, demo scripts and mock interviews.',
                      ParagraphStyle('CoverDescription', fontName='Body', fontSize=11.2, leading=16, textColor=MUTED))
        _, ph = p.wrap(BODY_WIDTH-5, 120)
        p.drawOn(c, x, y-492-ph)
        c.setFillColor(NAVY)
        c.setFont('Body-Bold', 10)
        c.drawString(x, 50, 'REVIEWED 11 NOVEMBER 2026')
        c.setFont('Body', 9)
        c.setFillColor(MUTED)
        c.drawString(x, 32, 'Local Windows verification  |  20 cases / 148 assertions / 41 CLI calls')
        c.drawString(x, 16, 'Current implementation and future work are explicitly distinguished.')

class BibleDoc(BaseDocTemplate):
    def __init__(self, filename):
        super().__init__(str(filename), pagesize=A4, leftMargin=LEFT, rightMargin=RIGHT,
                         topMargin=23*mm, bottomMargin=20*mm,
                         title='Groot - The Project Interview Bible', author='Project review and interview preparation',
                         subject='C++20 VCS implementation, CV audit and 170 answered interview questions')
        self.chapter = ''
        frame = Frame(LEFT, self.bottomMargin, BODY_WIDTH, HEIGHT-self.topMargin-self.bottomMargin,
                      leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0, id='body')
        self.addPageTemplates(PageTemplate(id='main', frames=[frame], onPage=self.decorate))

    def beforeDocument(self):
        self.chapter = ''

    def decorate(self, canvas, doc):
        if doc.page == 1:
            return
        canvas.saveState()
        canvas.setStrokeColor(colors.HexColor('#D4DFE5'))
        canvas.line(LEFT, HEIGHT-16*mm, WIDTH-RIGHT, HEIGHT-16*mm)
        canvas.setFont('Body-Bold', 8)
        canvas.setFillColor(TEAL)
        canvas.drawString(LEFT, HEIGHT-12.5*mm, 'GROOT  /  INTERVIEW BIBLE')
        canvas.setFont('Body', 8)
        canvas.setFillColor(MUTED)
        canvas.drawRightString(WIDTH-RIGHT, HEIGHT-12.5*mm, '11 NOV 2026')
        canvas.setStrokeColor(colors.HexColor('#D4DFE5'))
        canvas.line(LEFT, 14.8*mm, WIDTH-RIGHT, 14.8*mm)
        canvas.setFont('Body', 7.8)
        label = 'Source-grounded interview preparation'
        if len(label) > 78:
            label = label[:75] + '...'
        canvas.drawString(LEFT, 10.8*mm, label or 'Source-grounded interview preparation')
        canvas.setFont('Body-Bold', 9)
        canvas.setFillColor(NAVY)
        canvas.drawRightString(WIDTH-RIGHT, 10.8*mm, str(doc.page))
        canvas.restoreState()

    def afterFlowable(self, flowable):
        if getattr(flowable, '_chapter_title', None):
            title = flowable._chapter_title
            key = 'chapter-' + title.split(' | ')[0]
            self.chapter = title
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(title, key, 0, False)
            self.notify('TOCEntry', (0, title, self.page, key))
        elif getattr(flowable, '_question_id', None):
            self.canv.bookmarkPage(flowable._question_id)

def build():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    source = SOURCE.read_text(encoding='utf-8')
    story = [Cover(), PageBreak(), Paragraph('Contents', styles['chapter']),
             Paragraph('A complete preparation route from your CV claims to implementation details and interview practice. Page numbers and chapter names are clickable.', styles['body']), Spacer(1, 10)]
    toc = TableOfContents()
    toc.levelStyles = [styles['toc']]
    toc.dotsMinLevel = 0
    story.append(toc)
    story.append(PageBreak())
    source = source[source.index('# 01 |'):]
    lines = source.splitlines()
    paragraph = []
    code = []
    in_code = False
    first_chapter = True
    current_chapter = ''

    def flush():
        if paragraph:
            value = ' '.join(paragraph)
            style = styles['source'] if value.startswith('Evidence:') else styles['reference_body'] if current_chapter == '20' else styles['body']
            story.append(Paragraph(rich(value), style))
            paragraph.clear()

    for line in lines:
        if line.startswith('```'):
            flush()
            if in_code:
                font_size = 7.7
                widest = max((pdfmetrics.stringWidth(s, 'Mono', font_size) for s in code), default=0)
                if widest > BODY_WIDTH-22:
                    font_size *= (BODY_WIDTH-22)/widest
                style = ParagraphStyle('CodeFit', parent=styles['code'], fontSize=font_size, leading=font_size*1.4)
                story.append(Preformatted('\n'.join(code), style))
                code.clear()
            in_code = not in_code
            continue
        if in_code:
            code.append(line)
            continue
        if not line.strip():
            flush()
        elif line.startswith('# '):
            flush()
            if not first_chapter:
                story.append(PageBreak())
            first_chapter = False
            title = line[2:]
            number, heading = title.split(' | ', 1)
            current_chapter = number
            story.append(Paragraph('CHAPTER '+number, ParagraphStyle('Kicker', fontName='Body-Bold', fontSize=10, leading=13, textColor=TEAL, spaceAfter=9, keepWithNext=True)))
            p = Paragraph(rich(heading), styles['chapter'])
            p._chapter_title = title
            story.append(p)
        elif line.startswith('## '):
            flush()
            story.append(Paragraph(rich(line[3:]), styles['section']))
        elif line.startswith('### '):
            flush()
            value = line[4:]
            value = re.sub(r'^(Q\d+)\.', r'<font color="#007E83">\1.</font>', html.escape(value))
            p = Paragraph(value, styles['question'])
            p._question_id = line[4:].split('.')[0]
            story.append(p)
        elif line.startswith('- '):
            flush()
            style = styles['reference_bullet'] if current_chapter == '20' else styles['bullet']
            story.append(Paragraph('&#8226; '+rich(line[2:]), style))
        else:
            paragraph.append(line.strip())
    flush()
    BibleDoc(OUTPUT).multiBuild(story)
    print(OUTPUT)

if __name__ == '__main__':
    build()
