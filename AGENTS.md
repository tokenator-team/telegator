# Agent Guide for Telegator (Telegram Desktop fork)

Required reading is this file only. The Telegator section is the owner's fork
rules and wins over the condensed upstream Telegram Desktop rules after it.
`TELEGATOR.md` is a reference read on demand (pointers below).

## Telegator

Форк `telegramdesktop/tdesktop` для внутреннего использования (macOS + Windows):
кнопки действий в окне чата и фичи, которых нет в официальном клиенте. Владелец —
не программист: пиши ему коротко, по-русски, технические решения принимай сам.
Цели, решения, очередь фич — `docs/telegator/ROADMAP.md`. Наш код —
`Telegram/SourceFiles/telegator/`; GitHub (публичный) — `tokenator-team/telegator`,
`main`; Telegram — remote `upstream`, ветка `dev`, теги `vX.Y.Z`.

- Обычная просьба — задача Telegator по этому файлу и ROADMAP; соседнюю очередь
  `ai-tdesktop` не ищи и не требуй. Её навыки (`perform-task`, `continue`,
  `process-inbox`) — только по явной просьбе с названной задачей upstream или
  этим workflow; тогда сохраняй его записи, проверки безопасности и полный review.
  Обычной правке — проверки и review по риску, но правила сборки, платформы,
  установки, денег, доступа и concurrency сохраняются.
- Модульность — каждая правка файла Telegram есть будущий конфликт: логика,
  строки, стили, состояние — только в `Telegram/SourceFiles/telegator/`; в файлах
  Telegram — точка вызова в 1–3 строки с комментарием `// Telegator` (найти все:
  `grep -rn "// Telegator" Telegram/SourceFiles`); новые исходники — в
  `Telegram/cmake/telegator.cmake`, не в `Telegram/CMakeLists.txt`; вставляйся в
  центральную функцию (все исходящие звонки — `Calls::Instance::startOutgoingCall`),
  а не в каждую кнопку; из чужого форка (AyuGram и др.) бери нужное, отвязывай от
  их настроек, источник — в шапке файла (GPLv3). Унаследованные workflow Telegram
  отключены: не включай и не удаляй их файлы.
- Нельзя: коммитить api_id/api_hash и любые секреты; класть в репозиторий
  бизнес-специфику владельца — адреса серверов, названия действий, ключи (кнопки —
  универсальный механизм, конкретика — в локальном конфиге); сохранять контент с
  таймером самоуничтожения (п. 1.4 условий Telegram API, даже если так делает чужой
  форк); выполнять рабочие действия в клиенте — кнопка сообщает серверу владельца
  «нажали X в чате Y», решает и исполняет сервер. Исключение — «Реквизиты»
  (владелец 23.09.2026): оформление, справочник банков, отправка и закреп — в клиенте.
- Проверка без сборки — `make check` (§Быстрые проверки). Сборка — в `~/Projects/telegator-wt/dev`,
  Debug, только `nice -n 10` и `--parallel 4` (иначе Mac на 16 ГБ зависает);
  ключи API не печатать (их выводит configure).
- У владельца одна копия — `/Applications/Telegator.app`, ставит её только
  `tools/telegator/install_mac.sh`; копий в `builds/`, на рабочем столе, в `/tmp`
  не делать (владелец 25.09.2026). Свою Debug запускай, закрыв установленную, после
  проверки обязательно `install_mac.sh --restore`. Сообщения проверяй в
  «Избранном», не на живых собеседниках.
- В `main` — только `python3 ~/Projects/tokenator/scripts/merge.py telegator
  <ветка>`, без прямого коммита и push; сборку форка ворота не проверяют — проверь
  сам. `main` на GitHub не переписывать без явного согласия владельца. В git
  добавляй конкретные файлы; после слияния удали ветку локально и на GitHub.
- Сделанное — в «Сделано» ROADMAP; пометки `TEST-BUTTON-POC` и `// Telegator` не
  оставляй мёртвыми. Временные клоны и логи — в `/tmp`, удалить в конце задачи.

По нужде — `TELEGATOR.md`: библиотеки, configure, Release, облачная сборка
(§Сборка); папка данных, запуск, `-workdir` (§Запуск); dmg и Windows для
сотрудников (§Раздача); rebase на новый Telegram и конфликт в этом файле
(§Обновление с Telegram); «Реквизиты»; журнал действий; найдена старая сборка
с `tdata/` или `workdir/`, нет кеша Actions (§Гигиена).

## Where the full upstream text is

This file keeps the upstream Telegram Desktop rules every task needs, condensed
(updating it on rebases: `TELEGATOR.md` §Обновление с Telegram).
Examples and situational procedures stay in the upstream original, referred to
below as "upstream §Section":

```bash
git show $(git merge-base HEAD upstream/dev):AGENTS.md
```

Mechanical style rules with examples: `REVIEW.md`.

## Situational pointers

- Requested upstream queue work: upstream §AI Tasks (listing, urgency ranking,
  this checkout's tag), then the skill. Browsing is read-only; act only through
  the skills; no `python3 ai.py`; never take over another checkout's claimed
  work without human reassignment; never guess between similar task ids. A
  task-owned commit is three lines: subject, blank, `Task: <task-id>` (no
  rationale; no commit hashes in task artifacts).
- Codex on Windows over `\\wsl.localhost`: upstream §Working from Codex on
  Windows + WSL. Use WSL Git and tools; retry an odd UNC-directory command via
  `wsl.exe -d <distro> --cd <linux path> --` before calling it broken; never
  change global `safe.directory` unless asked or run Windows `cmake --build out`
  on a Linux/Docker tree; no Docker build unless the user asked.
- Toolchains: `docs/building-*.md` (current: Visual Studio 2026, not upstream's
  2022). Build layout, Native Tools prompts, "Libraries not found": upstream
  §Build System Structure to §Troubleshooting.
- macOS Debug log stops right after `Lang Info: Loaded cached, keys: ...` and a
  fresh `Telegram-*.ips` shows `SIGABRT` in `Lang::Instance::applyValue()` /
  `Local::readLangPack()`, twice: upstream §macOS crashes while reading the
  cached language pack — change no code/overlay/account, back up portable
  folders outside `out/`, one clean Debug rebuild, never loop.
- Build-output lock errors (PDB, EXE, OBJ): inside the autonomous `continue` /
  `perform-task` workflow follow `.agents/shared/build-lock-recovery.md`.
  Otherwise the checkout executable may be running because the user is testing
  it: do not terminate it or delete locked outputs without explicit permission;
  report the exact locked path and ask. Never stop an installed client, another
  checkout, an IDE or an unknown process.

## Build

- Agents build Debug; Release is extremely heavy and only for the owner/staff
  builds `TELEGATOR.md` describes. Version: `Telegram/build/version`.
- macOS, first configure of a new `out/`: `QT` names the installed
  `../Libraries/local/Qt-*` version (e.g. `export QT=6.11.1`). Reconfiguring an
  existing `out/`: never export `QT`, run `env -u QT cmake -S . -B out`. An
  exported `QT` is force-written into the cache; a mismatch fails regeneration,
  so new sources are silently not compiled (later undefined-symbol link errors)
  and new `.style` modules leave `style_*.h` includes stale.

## Debug-Only Code

Production translation units stay free of debug machinery. The permanent test
harness is `Telegram/SourceFiles/test/`; the disposable `-testagent` overlay owns
per-task instrumentation; production carries at most a thin single-call seam
(a `Test::Fire()`-style waitpoint or a live-object publication).

- No `#ifdef _DEBUG` blocks, debug-only types, state, mutexes, counters or
  observation structs in production headers or sources. If behavior cannot be
  observed without them, extend `test/` helpers or the overlay instead.
- Never commit debug machinery interleaved with working code in one unit:
  permanent helpers go under `test/`; temporary ones in the overlay, never kept.
- The few deliberate `#ifdef _DEBUG` hooks are exceptions; each new one needs a
  solid stated reason. "The test needed it" is not one.

## Text File Format

UTF-8 without BOM for source, header, build/config, style and localization
files. Native (non-WSL) Windows checkouts keep CRLF; WSL/Linux checkouts stay
LF-only unless a file already uses another convention — never let Windows tools
rewrite them to CRLF. When normalizing, change nothing else and add no BOM.

## Commits

- Subject: one concise plain-language line, ~50-60 characters, in the style of
  recent `git log`; usually the whole message. For ordinary work add a body of a
  line or two only when the subject can't carry it (what, not how).
- `[ai] ` prefix, decided per commit: only when every retained change and the
  purpose are exclusively the AI workflow (harness, skills, prompts, commands,
  agent docs, AI testing infrastructure, e.g. `Telegram/SourceFiles/test/`,
  `.agents/`, `.claude/`, `.grok/`, `AGENTS.md`, `CLAUDE.md`, `GROK.md`; paths
  alone don't decide). Product test seams, app code and build integration don't
  qualify. Split mixed commits when practical, else no prefix. The overlay and
  external task artifacts don't count. No other commit contains `[ai]`
  anywhere. The prefix marks scope, never authorship.
- Never add `Co-Authored-By:` or any tool/assistant trailer, `Autotask:`,
  attempt or other run markers.

## Local Storage Serialization

`Core::Settings` and `Main::SessionSettings` serialize sequentially via
`QDataStream`. Append new fields only at the end of the stream, never in the
middle (older saved data would be misread); read them behind
`!stream.atEnd()` with a meaningful default. For simple flags and values prefer
the KV prefs `writePref<Type>` / `readPref<Type>` instead.

## Coding Style

- Comments are rationed: a comment is one line; two or three only when the
  block opens with `// WHY:`. A commit may add two comment lines plus one such
  exception; a trailing comment is a line too; only `} // namespace X` and
  `#endif // X` labels are free. Say why, never what. Hooks enforce it;
  `Telegram/SourceFiles/test/` is exempt. Do not remove existing comments to
  satisfy this: keep them unless your change makes them wrong or obsolete, and
  move them with moved code. Labels for positional arguments of generated
  TL/MTP constructors are useful.
- `REVIEW.md` holds the mechanical rules: empty line before closing brace,
  operator placement, if-with-initializer, trailing return types only when
  the normal form is too long, `tr::` projections, `Q_OS_LINUX`, and more.
- Never discard a result with `static_cast<void>(...)` or `(void)expr`; fix the
  design instead of silencing `[[nodiscard]]`.
- Prefer `auto` / `const auto` / `const auto &` over explicit types.
- Use `u"..."_q`, not `QStringLiteral("...")`.
- Platforms are Windows / macOS / all-other. New code never uses `Q_OS_LINUX`:
  write `!defined Q_OS_WIN && !defined Q_OS_MAC` or runtime
  `Platform::IsLinux()`, which means exactly that. CMake `LINUX` is likewise
  `NOT WIN32 AND NOT APPLE`: split `if (WIN32)` / `elseif (APPLE)` / `else()`,
  never add a fallback branch after `if (LINUX)`.
- All-other-platform GLib/GObject/GIO code uses cppgir `gi::repository`
  wrappers; raw `g_*` only where cppgir lacks the feature or at a narrow
  interop boundary.
- A D-Bus interface known at build time: typed bindings via `generate_dbus` from
  `cmake/external/glib/generate_dbus.cmake` (upstream §Coding Style); generic
  GDBus calls only for genuinely dynamic interfaces.

## API Usage

Schemas: `Telegram/SourceFiles/mtproto/scheme/api.tl` (Telegram API, the source
for method signatures and return types) and `mtproto.tl` (protocol). Request
pattern with examples: upstream §Making API Requests.

- Several constructors: `.match(...)`, or `.type() == mtpc_x` then
  `.c_x()`; a single constructor: `.data()`.
- `.handleFloodErrors()` before `.send()` only in rare special flood handling.
- Silently ignore 406 in UI: guard toasts with `MTP::IgnoreError(error)` or use
  `MTP::ShowErrorFallback(show, error)` (`mtproto/mtproto_response.h`).
- `.done()` / `.fail()` handlers live as long as the session, not their
  creator: capturing a widget, box, controller or other shorter-lived state is
  a use-after-free. Plain values and session-owned objects are fine; otherwise:
  1. `crl::guard(this, ...)` — request always sent, handler skipped when the
     guard is dead; use when the server must see the request. Guards (`this`
     of a `QObject` or `base::has_weak_ptr`, weak/shared pointers): upstream
     §API Request Callback Lifetime.
  2. Keep the `mtpRequestId`; `api().request(base::take(_requestId)).cancel()`
     when irrelevant and in the destructor. The request may never be sent.
  3. Own an `MTP::Sender` (`_api(&session->mtp())`): its destructor cancels its
     requests; same caveat. Prefer it over (2) for more than a request or two.

## UI Styling

`.style` syntax, built-in types and usage: upstream §UI Styling.

- Never hardcode sizes: margins, paddings, spacing, sizes, coordinates go to
  `.style` files and are read via `st::`; raw numbers break non-100% scale.
- Animation durations are not style values: `constexpr auto kName =
  crl::time(N)` in an anonymous namespace of the `.cpp`.

## Localization

Strings: `Telegram/Resources/langs/lang.strings` (`#one` / `#other` plural
keys). Examples: upstream §Localization.

- `tr::lng_x(tr::now, ...)` gives a `QString`; without `tr::now` an
  `rpl::producer<QString>`.
- Placeholders: `lt_tag, value`; `{count}` takes `int` immediately, or
  `rpl::producer<float64>` with `| tr::to_count()` reactively. `std::move`
  producers passed as placeholders.
- `tr::marked`, `tr::rich`, `tr::bold`, `tr::italic`, `tr::underline`,
  `tr::link`, `tr::url(...)` work as placeholder wrappers and, as the last
  argument, project to `TextWithEntities`; always prefer them over
  `Ui::Text::*` (`REVIEW.md`).

## RPL

Examples: upstream §RPL. `std::move` producers when starting pipelines; pass an
`rpl::lifetime` to `rpl::on_next` / `on_error` / `on_done` /
`on_next_error_done` or store the returned one; `rpl::duplicate` to reuse a
producer; `rpl::combine` unpacks latest values into lambda arguments.
