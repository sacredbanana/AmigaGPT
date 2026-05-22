# Unicode, Zeichensätze und Anzeige auf MorphOS (AmigaGPT)

Ergänzt [SCINTILLA-ARCHITECTURE.md](SCINTILLA-ARCHITECTURE.md). Beschreibt, warum der Chat heute über `codesets.library` und **NFloattext** läuft, was auf MorphOS geprüft wurde, und warum **Scintilla.mcc + TTEngine** (nicht Cairo) das Ziel für verlustfreie UTF-8-Anzeige ist.

---

## 1. Drei getrennte Text-Pipelines

| Stelle | Widget | Richtung | Anmerkung |
|--------|--------|----------|-----------|
| **Chat-Ausgabe** | NFloattext (in NListview) | UTF-8 (`raw_utf8`) → `CodesetsUTF8ToStr` → System → Anzeige | Emoji-Test: `??` |
| **Chat-Eingabe** | `TextEditor` / `AmigaGPTTextEditor` | System → `CodesetsUTF8Create` → UTF-8 → API | **Nicht** Phase 6–10 / 12 Scope für Editor-Umstellung |
| **Code-Viewer** | NFloattext (interim) | `raw_code` UTF-8 | **Phase 6–10:** Scintilla + TTEngine |
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

---

## 4. Scintilla + TTEngine vs. Cairo

| Kriterium | Scintilla + TTEngine | Cairo |
|-----------|----------------------|--------|
| Branch-Ziel Code-Viewer (6–10) | **Ja** | Nein |
| UTF-8 ohne NFloattext/codesets | **Ja** (im Scintilla-Fenster) | **Ja**, wenn selbst gebaut |
| Aufwand in AmigaGPT | Mittel (eine MCC) | Hoch (eigenes Layout, Scroll, MUI-Glue) |
| Chat `??` beheben | Nur mit **Chat-Phase 12** | Nur wenn **gesamte** Chat-Ausgabe über Cairo |

**Entscheidung:** **Cairo** bewusst nicht geplant (zu komplex für MUI-Chat). **Phase 6–10** = Code-Viewer; **Phase 12** = Hauptfenster-Chat-Ausgabe auf Scintilla.

**Worker (Phase 13)** bewusst **nach** Chat-Scintilla (nicht davor), damit Batching den neuen Stream-Pfad optimiert.

---

## 5. Roadmap-Bezug (Phasen)

| Phase | Inhalt |
|-------|--------|
| 1–5 | Erledigt: Datenmodell, Stream, Fences, NFloattext interim, NList `name_list_display` |
| **6–10** | **Ein Implementierungsblock:** Code-Viewer Scintilla v0.1 |
| 11 | Komfort am Code-Viewer (Lexer, Highlighting, …) |
| **12** | Hauptfenster **Chat-Ausgabe** → Scintilla (+ TTEngine); Eingabe-Editor unverändert |
| **13** | MorphOS Worker / UI-Batching (altes „Phase 12“) |

**Nicht Phase 6–12:** Chat-Eingabe (`TextEditor`), Cairo, NList-Titel (bleibt wie implementiert).

---

## 6. Kurzfassung

AmigaGPT speichert in **UTF-8**. **NFloattext** und die klassische MUI-Kette erwarten **System-Codeset-Strings** → auf typischem MorphOS **Konvertierungsverluste** (Prüfung: Emoji → `??`). **Scintilla.mcc mit TTEngine** umgeht das für Code (6–10) und geplant für die Chat-**Ausgabe** (12). **Cairo** wäre ein separater, schwerer Gesamt-Renderer.
