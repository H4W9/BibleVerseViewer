#pragma once
// bible_data.h — no verse data here anymore.
// All verses are loaded at runtime from SD card text files:
//   /ext/apps_data/bible_viewer/verses_en.txt  (KJV English)
//   /ext/apps_data/bible_viewer/verses_esv.txt  (ESV English)
//   /ext/apps_data/bible_viewer/verses_de.txt  (Luther 1912 German)
//
// File format — one verse per line, pipe-delimited:
//   Reference|Book|Verse text
//   e.g.  John 3:16|John|For God so loved the world...
//
// You can edit these files in any plain text editor, add or remove
// verses, or create additional language files named verses_xx.txt
// and they will appear automatically in the Settings menu.
