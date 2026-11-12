# Swarm Phase 1: lab briefing slides

Slides for the instructor to present at the start of the Swarm Phase 1 lab
(what the project is, how it works, what to do, how it is graded).
The lab handout itself is `../swarm-phase1-threads-sync.md`.

Available in English and Turkish (same deck, translated; technical terms and
identifiers stay in English).

## PDFs (ready to use)

| File | Content |
|---|---|
| `swarm-phase1-lab-briefing-en.pdf` | English slides, 16 pages, for projecting |
| `swarm-phase1-lab-briefing-tr.pdf` | Turkish slides, 16 pages, for projecting |
| `swarm-phase1-lab-notes-en.pdf` | English slides with the notes to the students under each slide |
| `swarm-phase1-lab-notes-tr.pdf` | Turkish slides with the notes (Notlar) under each slide |

The notes are written for the students (they explain each slide in words, so
the deck can be read without the presenter). In the PDFs the animated show on
the result slide is a still of the final frame; the animation is
`../images/swarm/swarm-show.gif`.

## Source

`en/project/` and `tr/project/` hold the deck in the Slides-artifact format
(1920×1080 slides): `deck.json` plus one HTML file per slide in
`slides/`, with the notes in each slide's `<aside>`. Images are referenced as
`/_blob/<id>`; `assets.json` maps each id to a local file under
`../images/swarm/`.

To edit: change the HTML in `en/project/slides/` or `tr/project/slides/`, then
rebuild the PDFs.

## Rebuild the PDFs

Needs Node.js with Playwright (Chromium) installed globally, and network
access for the Google Fonts (Space Grotesk, Inter, JetBrains Mono).

```
cd labs/swarm-phase1-lab-slides
NODE_PATH=$(npm root -g) node build_pdf.js en
NODE_PATH=$(npm root -g) node build_pdf.js tr
```

Each run writes `swarm-phase1-lab-briefing-<lang>.pdf` and
`swarm-phase1-lab-notes-<lang>.pdf`.
