# Unicode, Zeichensätze und Anzeige auf MorphOS (AmigaGPT)

Ergänzt [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md). Beschreibt, warum der Chat heute über `codesets.library` und **NFloattext** läuft, was auf MorphOS geprüft wurde, und warum **Scintilla.mcc + TTEngine** (nicht Cairo) das Ziel für verlustfreie UTF-8-Anzeige ist.

---

## 1. Drei getrennte Text-Pipelines

| Stelle | Widget | Richtung | Anmerkung |
|--------|--------|----------|-----------|
| **Chat-Ausgabe** | MorphOS: Scintilla + TTEngine (Scrollgroup); OS3/OS4: NFloattext | MorphOS: UTF-8 direkt; OS3/OS4: codesets | MorphOS Phase 12: [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md) |
| **Chat-Eingabe** | `TextEditor` / `AmigaGPTTextEditor` | System → `CodesetsUTF8Create` → UTF-8 → API | **Nicht** Phase 6–10 / 12 Scope für Editor-Umstellung |
| **Code-Viewer** | Scintilla + TTEngine (MorphOS, ab Phase 6/7) | `raw_code` UTF-8 direkt | **Erledigt** auf MorphOS; OS3/OS4 noch NFloattext |
| **NList-Titel** | NList | `name` UTF-8; Anzeige `name_list_display` | **Bereits umgesetzt** (`conversationRefreshNameListDisplay()` in `gui.c`) |

**Speicher/API:** `raw_utf8`, JSON, `utf8stream` sind durchgehend UTF-8 (verlustfrei in RAM).

---

## 2. System-Zeichensatz und `codesets.library`

AmigaGPT ermittelt beim Start:

```c
systemCodeset = CodesetsFindA(NULL, NULL);
```

`NULL` = Standard-Codeset von **codesets.library** (nicht im Quellcode fest auf ISO-8859-15 verdrahtet).

### NFloattext

- API (`NFloattext_mcc.h`): nur `MUIA_NFloattext_Text` (`STRPTR`), MUI-Einbettungscodes (`\033…`).
- **Kein** UTF-8-Attribut — erwartet Bytes im **System-/App-Codeset**, nicht rohes UTF-8.

### `codesets.library`

- Wandelt zwischen UTF-8 und registrierten Codesets (u. a. ISO-8859-15, UTF-8).
- **Verlust** wenn das Ziel-Codeset ein Zeichen nicht darstellt (`CSA_MapForeignChars` → oft `?`).

`struct codeset` mit `table[256]` zeigt: die klassische Welt ist **8-Bit-orientiert**. UTF-8 ist als MIBenum vorhanden, ist aber nicht auf jedem System der Default.

### MorphOS ohne TTEngine (klassischer Pfad)

- `graphics.library` / `Text()`: praktisch 8-Bit-Codeset.
- MUI-Standardgadgets (NFloattext, viele Listen): Strings im lokalen Charset.
- **TTEngine** (`ttengine.library`, FreeType): UTF-8/Unicode-Ausgabe ins `RastPort` — von **Scintilla.mcc** (und spezialisierten Apps) genutzt, nicht von NFloattext.

### Zwei Font-Listen (wichtig für Tests)

| Modus | Typische Namen | Wo sichtbar |
|-------|----------------|-------------|
| **graphics.library / MUI** | Andale Mono, Arial, Times New Roman, Verdana, … | MUI-Font-Auswahl, klassische Apps |
| **TTEngine** (FreeType) | DejaVu Sans Mono, Bitstream Vera Sans Mono, Liberation Mono, Noto Sans Mono, Roboto Mono, Luxi Mono, Arimo, … | Apps mit TTEngine (z. B. **Flow Studio** mit TTEngine aktiv), **Scintilla** mit `SCI_SETFONTQUALITY(1)` |

AmigaGPT Code-Viewer: `SCI_SETFONTQUALITY(1)` + `ttengine.library` → **`SCI_STYLESETFONT`** muss einen **TTEngine-Family-Namen** verwenden (Default in `CodeBlocksScintilla.c`: `DejaVu Sans Mono`), nicht zwingend die kurze MUI-Liste.

**locale.library** kann UTF-8 verarbeiten; das ersetzt nicht die NFloattext-Pipeline.

---

## 3. MorphOS-Prüfung (dokumentiert)

| Prüfung | Ergebnis |
|---------|----------|
| Shell: `GETENV Charset` | **Objekt nicht gefunden** — Variable nicht gesetzt |
| Emoji im Chat (NFloattext) | **`??`** |
| Interpretation | Typisch **8-Bit-System-Codeset** + `codesets`-Ersatz, nicht „nur fehlende Font-Glyphe“ (dafür eher leeres Kästchen □) |

**Hinweis:** Fehlende ENV-Variable `Charset` bedeutet nicht „kein Codeset“ — `codesets.library` nutzt trotzdem einen internen Standard (auf westlichen MorphOS-Setups häufig ISO-8859-15).

Optional in der Shell: `GETENV Language`, `GETENV Country` (Locale-Preferences setzen oft Sprache/Land, aber kein sichtbares „Codeset“-Feld).

### Emoji testen (Referenz)

1. **Ansicht → Markdown formatting** aus (sauberer Test).
2. Emoji ins Eingabefeld einfügen (z. B. aus Browser/Zwischenablage) oder API: „Antworte nur mit: 😀“.
3. **Return** = senden; **Shift+Return** = neue Zeile ([`AmigaGPTTextEditor`](../src/AmigaGPTTextEditor.c)).

**MorphOS Chat (Phase 12 + Midi-Markdown):** Farb-Emoji sind mit TTEngine/DejaVu nicht darstellbar (Flow Studio: andere Fonts helfen kaum; Stylos/Cairo nur teilweise, nicht farbig). Mit **Markdown formatting an** ersetzt AmigaGPT häufige Emoji in der **Assistant-Anzeige** durch kurze Texte (z. B. 🌍→`[Welt]`, 👍→`(+1)`); **Export raw UTF-8** und `raw_utf8` bleiben original. **Markdown aus** → keine Ersetzung (oft □ oder fehlende Glyphe).

---

## 4. Scintilla + TTEngine vs. Cairo

| Kriterium | Scintilla + TTEngine | Cairo |
|-----------|----------------------|--------|
| Branch-Ziel Code-Viewer (6–7b MorphOS) | **Ja** (umgesetzt) | Nein |
| UTF-8 ohne NFloattext/codesets | **Ja** (im Scintilla-Fenster) | **Ja**, wenn selbst gebaut |
| Aufwand in AmigaGPT | Mittel (eine MCC) | Hoch (eigenes Layout, Scroll, MUI-Glue) |
| Chat `??` beheben | Nur mit **Chat-Phase 12** | Nur wenn **gesamte** Chat-Ausgabe über Cairo |

**Entscheidung:** **Cairo** bewusst nicht geplant (zu komplex für MUI-Chat). **Code-Viewer** auf MorphOS: Scintilla + TTEngine (**6, 7a, 7b erledigt**). **Phase 12** = Hauptfenster-Chat-Ausgabe auf Scintilla. Stream/Chat-Sync: [STREAM-RECOVERY.md](STREAM-RECOVERY.md).

**Worker (Phase 13)** bewusst **nach** Chat-Scintilla (nicht davor), damit Batching den neuen Stream-Pfad optimiert.

---

## 5. Roadmap-Bezug (Phasen)

| Phase | Inhalt |
|-------|--------|
| 1–5 | Erledigt: Datenmodell, Stream, Fences, NFloattext interim, NList `name_list_display` |
| **6, 7a, 7b** | **Erledigt (MorphOS):** Code-Viewer Scintilla, NList, Copy/Save UTF-8 + System |
| **6–10** | Code-Viewer v0.1 ✓ — [PHASE-10-DOD.md](PHASE-10-DOD.md) |
| **R1–R4** | Stream- & Chat-Recovery — [STREAM-RECOVERY.md](STREAM-RECOVERY.md) |
| 11 | Code-Viewer Lexer ✓ — [PHASE-11-LEXER.md](PHASE-11-LEXER.md) |
| **12** | Chat-Ausgabe Scintilla — [PHASE-12-CHAT-SCINTILLA.md](PHASE-12-CHAT-SCINTILLA.md) (MorphOS implementiert, Test ausstehend) |
| **13** | MorphOS Worker / UI-Batching (altes „Phase 12“) |

**Nicht Phase 6–12:** Chat-Eingabe (`TextEditor`), Cairo, NList-Titel (bleibt wie implementiert).

---

## 6. Kurzfassung

AmigaGPT speichert in **UTF-8**. **NFloattext** (OS3/OS4 Chat) erwartet **System-Codeset-Strings** → Konvertierungsverluste (Prüfung: Emoji → `??`). Auf **MorphOS** nutzt die Chat-**Ausgabe** seit **Phase 12** **Scintilla + TTEngine** mit UTF-8 direkt; der **Code-Viewer** tat das bereits ab Phase 6. **Cairo** wäre ein separater, schwerer Gesamt-Renderer.
