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

Condensed upstream rules follow; rebase updates: `TELEGATOR.md` §Обновление с
Telegram. For examples and procedures, "upstream §Section" refers to:

```bash
git show $(git merge-base HEAD upstream/dev):AGENTS.md
```

## Situational pointers

- Requested upstream queue work: upstream §AI Tasks (listing, urgency, checkout
  tag), then the skill. Browse read-only; act only via skills, never `python3
  ai.py`. No takeover of another checkout's claimed work without human
  reassignment; never guess similar task ids.
  Task commit: exactly three lines — subject, blank, `Task: <task-id>` (no
  rationale); no commit hashes in task artifacts.
- Codex on Windows over `\\wsl.localhost`: upstream §Working from Codex on
  Windows + WSL. Use WSL Git/tools; retry a failing UNC-directory command via
  `wsl.exe -d <distro> --cd <linux path> --` before calling it broken; never
  change global `safe.directory` unless asked. Never run Windows `cmake --build
  out` on a Linux/Docker tree; no Docker build unless asked.
- Toolchains: `docs/building-*.md` (Visual Studio 2026, not upstream's 2022).
  Build layout, Native Tools prompts, "Libraries not found": upstream
  §Build System Structure to §Troubleshooting.
- macOS Debug twice stops after `Lang Info: Loaded cached, keys: ...` with fresh
  `Telegram-*.ips` showing `SIGABRT` in `Lang::Instance::applyValue()` /
  `Local::readLangPack()`: upstream §macOS crashes while reading the cached
  language pack. No code/overlay/account changes; back up portable folders
  outside `out/`, one clean Debug rebuild, no loop.
- Locked PDB/EXE/OBJ: autonomous `continue` / `perform-task` uses
  `.agents/shared/build-lock-recovery.md`. Otherwise report the exact locked
  path; require explicit permission to stop the checkout executable or delete
  locked outputs: the user may be testing. Never stop an installed client, another
  checkout, IDE or unknown process.

## Build

- Agents build Debug; heavy Release is only for owner/staff per `TELEGATOR.md`.
  Version: `Telegram/build/version`.
- macOS new `out/`: set `QT` to installed `../Libraries/local/Qt-*` version
  (e.g. `export QT=6.11.1`). Existing `out/`: never export `QT`; use
  `env -u QT cmake -S . -B out`. Export overwrites the cache; mismatch breaks
  regeneration, silently omitting new sources (undefined-symbol links) and
  leaving `.style` modules' `style_*.h` includes stale.

## Debug-Only Code

Permanent harness/helpers: `Telegram/SourceFiles/test/`; per-task instrumentation:
disposable `-testagent` overlay, never retained. Production headers/sources carry
at most a thin single-call seam (`Test::Fire()` waitpoint or live-object
publication), never interleaved debug machinery: no debug-only types, state,
mutexes, counters, observation structs or `#ifdef _DEBUG` blocks. Extend helpers
or overlay to observe behavior. Deliberate `_DEBUG` hooks are exceptions; each
new one needs a solid stated reason beyond "The test needed it".

## Text File Format

UTF-8 without BOM for source, header, build/config, style and localization
files. Native (non-WSL) Windows checkouts keep CRLF; WSL/Linux checkouts stay
LF-only unless a file already uses another convention — never let Windows tools
rewrite them to CRLF. When normalizing, change nothing else and add no BOM.

## Commits

- Subject: one plain-language line, ~50-60 characters, recent `git log` style;
  usually sufficient. Ordinary work: body of 1-2 lines only if needed (what, not how).
- `[ai] ` marks scope, never authorship: per commit, purpose and every retained
  change must be exclusively AI workflow (harness, skills, prompts, commands,
  agent docs/testing infrastructure). Paths alone don't decide. Product test
  seams, app code/build integration don't qualify. Split mixed commits when
  practical, else no prefix; ignore overlay/external task artifacts. No other
  commit may contain `[ai]` anywhere.
- No `Co-Authored-By:`, tool/assistant trailers, `Autotask:`, attempt/run markers.

## Local Storage Serialization

`Core::Settings` and `Main::SessionSettings` serialize sequentially via
`QDataStream`. Append new fields only at the end of the stream, never in the
middle (older saved data would be misread); read them behind
`!stream.atEnd()` with a meaningful default. For simple flags and values prefer
the KV prefs `writePref<Type>` / `readPref<Type>` instead.

## Coding Style

- Comments: one line; 2-3 only starting with `// WHY:`. Per commit: two lines
  plus one such exception; trailing comments count, only `} // namespace X` /
  `#endif // X` labels are free. Say why, not what. Hooks enforce this;
  `Telegram/SourceFiles/test/` is exempt. Keep existing comments unless made
  wrong/obsolete by your change; move with code, never delete to meet quota.
  Generated TL/MTP constructor positional-argument labels are useful.
- Mechanical rules and examples: `REVIEW.md`.
- Never discard a result with `static_cast<void>(...)` or `(void)expr`; fix the
  design instead of silencing `[[nodiscard]]`.
- Prefer `auto` / `const auto` / `const auto &` over explicit types.
- Use `u"..."_q`, not `QStringLiteral("...")`.
- Platforms: Windows / macOS / all-other. New code: never `Q_OS_LINUX`; use
  `!defined Q_OS_WIN && !defined Q_OS_MAC` or equivalent `Platform::IsLinux()`.
  CMake `LINUX` = `NOT WIN32 AND NOT APPLE`: `if (WIN32)` / `elseif (APPLE)` /
  `else()`, never fallback after `if (LINUX)`.
- All-other GLib/GObject/GIO: cppgir `gi::repository`; raw `g_*` only for missing
  cppgir features or narrow interop.
- Build-time-known D-Bus interfaces: typed `generate_dbus` bindings from
  `cmake/external/glib/generate_dbus.cmake` (upstream §Coding Style); generic
  GDBus only for genuinely dynamic interfaces.

## API Usage

Signatures/return types: `Telegram/SourceFiles/mtproto/scheme/api.tl` (API),
`mtproto.tl` (protocol). Request examples: upstream §Making API Requests.

- Several constructors: `.match(...)`, or `.type() == mtpc_x` then
  `.c_x()`; a single constructor: `.data()`.
- `.handleFloodErrors()` before `.send()` only in rare special flood handling.
- Silently ignore 406 in UI: guard toasts with `MTP::IgnoreError(error)` or use
  `MTP::ShowErrorFallback(show, error)` (`mtproto/mtproto_response.h`).
- `.done()` / `.fail()` live for the session: capturing shorter-lived widgets,
  boxes, controllers/state causes use-after-free. Values/session-owned objects
  are safe; otherwise choose:
  1. `crl::guard(this, ...)`: always sends, skips handler after guard death; use
     when server must see request. Guards (`QObject`/`base::has_weak_ptr` `this`,
     weak/shared pointers): upstream §API Request Callback Lifetime.
  2. Store `mtpRequestId`; cancel when irrelevant AND in destructor via
     `api().request(base::take(_requestId)).cancel()`; may prevent sending.
  3. Own `MTP::Sender` (`_api(&session->mtp())`): destructor cancels requests,
     may prevent sending; prefer over (2) for more than 1-2 requests.

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
