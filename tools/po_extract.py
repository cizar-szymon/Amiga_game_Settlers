"""The texts to translate of the Amiga port: _("..."), N_("..."), C_("ctx",
"...") and NC_("ctx", "...") in freeserf/src and port, with the string
pieces next to each other joined as the compiler does.

  python tools/po_extract.py              -> port/po/settlers.pot
  python tools/po_extract.py --update     also adds the new texts (empty) to
                                           port/po/*.po and lists the ones
                                           no longer used
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = (glob.glob(os.path.join(ROOT, 'freeserf', 'src', '*.cc')) +
           glob.glob(os.path.join(ROOT, 'freeserf', 'src', '*.h')) +
           glob.glob(os.path.join(ROOT, 'port', '*.cc')))
# Only what the Amiga build has (the SDL files are not in it).
SKIP = ('-sdl', 'video-sdl', 'audio-sdlmixer', 'translation.cc', 'freeserf.cc')

STRING = r'"((?:[^"\\]|\\.)*)"'
PIECES = r'(?:\s*"(?:[^"\\]|\\.)*")+'
CALL = re.compile(r'\b(N?C?_)\(\s*(' + PIECES + r')\s*(?:,\s*(' + PIECES + r'))?\s*\)', re.S)


def join(pieces):
    return ''.join(re.findall(STRING, pieces))


def extract():
    texts = {}
    for path in sorted(SOURCES):
        if any(k in os.path.basename(path) for k in SKIP):
            continue
        src = open(path, encoding='latin-1').read()
        for m in CALL.finditer(src):
            kind = m.group(1)
            if kind in ('C_', 'NC_'):
                if m.group(3) is None:
                    continue
                ctxt, msgid = join(m.group(2)), join(m.group(3))
            else:
                if m.group(3) is not None:
                    continue
                ctxt, msgid = '', join(m.group(2))
            if not msgid:
                continue
            line = src.count('\n', 0, m.start()) + 1
            texts.setdefault((ctxt, msgid), []).append(
                '%s:%d' % (os.path.relpath(path, ROOT).replace('\\', '/'), line))
    return texts


def po_quote(s):
    return '"' + s + '"'


def write_pot(texts, path):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('# The texts of the Settlers port (Freeserf) to translate.\n'
                'msgid ""\nmsgstr ""\n'
                '"Content-Type: text/plain; charset=UTF-8\\n"\n\n')
        for (ctxt, msgid), refs in sorted(texts.items(), key=lambda t: t[1][0]):
            f.write('#: %s\n' % ' '.join(refs[:3]))
            if ctxt:
                f.write('msgctxt %s\n' % po_quote(ctxt))
            f.write('msgid %s\nmsgstr ""\n\n' % po_quote(msgid))


def read_po_keys(path):
    keys = set()
    ctxt = None
    for line in open(path, encoding='utf-8'):
        if line.startswith('msgctxt '):
            ctxt = re.findall(STRING, line)[0]
        elif line.startswith('msgid '):
            keys.add((ctxt or '', re.findall(STRING, line)[0]))
            ctxt = None
    return keys


if __name__ == '__main__':
    texts = extract()
    os.makedirs(os.path.join(ROOT, 'port', 'po'), exist_ok=True)
    write_pot(texts, os.path.join(ROOT, 'port', 'po', 'settlers.pot'))
    print('%d texts -> port/po/settlers.pot' % len(texts))
    if '--update' in sys.argv:
        for po in glob.glob(os.path.join(ROOT, 'port', 'po', '*.po')):
            have = read_po_keys(po)
            new = [k for k in texts if k not in have]
            gone = [k for k in have if k not in texts and k[1]]
            with open(po, 'a', encoding='utf-8', newline='\n') as f:
                for ctxt, msgid in new:
                    if ctxt:
                        f.write('msgctxt %s\n' % po_quote(ctxt))
                    f.write('msgid %s\nmsgstr ""\n\n' % po_quote(msgid))
            print('%s: %d new, %d no longer used: %s' %
                  (os.path.basename(po), len(new), len(gone),
                   '; '.join(k[1] for k in gone[:10])))
