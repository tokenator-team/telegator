# Telegator — справка для агента

Читай по нужде: обязательные правила форка — раздел «Telegator» в `AGENTS.md`,
там же указатели, когда открывать разделы этого файла.

## Быстрые проверки

`make check` из корня — без сборки клиента и установки зависимостей:
`bash -n` для `tools/telegator/*.sh`, разбор наших `telegator-*.yml`
через Python с PyYAML или штатный Ruby с YAML. `plutil` YAML не разбирает.
Готовый `out/Debug/test_telegator_requisites` запускается с приватными
эталонами из раздела «Реквизиты»; если теста или эталонов нет, выводится
`SKIP`. Пути можно задать через `TELEGATOR_REQUISITES_TEST` и
`TELEGATOR_REQUISITES_VECTORS`. Ошибка доступной проверки останавливает команду.

## Сборка

Локально на Mac владельца (Xcode 27). Облако — запасной путь.

**Библиотеки** — готовь, когда они нужны для сборки (обычно после изменения
`Telegram/build/prepare/prepare.py` или при потере `Libraries/`; ~1 час, 27 ГБ):

```bash
cd ~/Projects/telegator-wt/dev && ./Telegram/build/prepare/mac.sh skip-release silent
```

- Скрипт кладёт `Libraries/` и `ThirdParty/` в папку над своей копией
  репозитория, то есть в `~/Projects/telegator-wt/`, общие для всех worktree.
  Не запускай его из `~/Projects/telegator`: библиотеки лягут в `~/Projects`.
- Xcode 27 собирает только под macOS 12.0 и новее: в `prepare.py` версия
  поднята до 12.0 (метка `# Telegator`), клиенту — `CMAKE_OSX_DEPLOYMENT_TARGET`.
- С 25.09.2026 библиотеки собраны без `skip-release` — есть и Debug, и
  Release (`mac.sh silent`). Debug — для разработки агентами (быстрее
  пересборка); владельцу и сотрудникам — только Release: Debug без
  оптимизации заметно дёргает меню и настройки.

**Клиент** (первый раз ~40 минут, дальше — только изменённое):

```bash
cd ~/Projects/telegator-wt/dev/Telegram
set -a; . ~/Projects/telegator-wt/telegram_api.env; set +a
./configure.sh -D CMAKE_CONFIGURATION_TYPES=Debug -D CMAKE_COMPILE_WARNING_AS_ERROR=OFF -D CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON -D DESKTOP_APP_DISABLE_CRASH_REPORTS=ON -D CMAKE_OSX_DEPLOYMENT_TARGET=12.0 -D CMAKE_CXX_FLAGS=-DMETA_NO_STD_FORWARD_DECLARATIONS -D TDESKTOP_API_ID="$TDESKTOP_API_ID" -D TDESKTOP_API_HASH="$TDESKTOP_API_HASH"
nice -n 10 cmake --build ../out --config Debug --parallel 4
```

Release для владельца: в configure — `-D "CMAKE_CONFIGURATION_TYPES=Debug;Release"`,
сборка — `--config Release`, результат — `out/Release/Telegator.app`
(Apple Silicon и Intel, `-O3`).

- `--parallel 4` и `nice`: на Mac владельца 16 ГБ памяти, без ограничения
  Xcode запускает ~20 компиляторов, Mac уходит в подкачку и зависает.
- Результат — `out/Debug/Telegator.app` внутри worktree. Configure нужен один
  раз на worktree; дальше только `cmake --build`.
- `META_NO_STD_FORWARD_DECLARATIONS` — штатный выключатель range-v3: её
  предварительные объявления `std::` не собираются с libc++ из Xcode 27.
- Ключи Telegram API — `~/Projects/telegator-wt/telegram_api.env` (вне git,
  права 600), строки `TDESKTOP_API_ID=` и `TDESKTOP_API_HASH=`. Те же ключи —
  в секретах GitHub. Значения не печатать: вывод configure их содержит.

**Облако**: GitHub Actions → **Telegator macOS**, ~85 минут, артефакт
`Telegator-macos` (zip с `.app`).

```bash
gh workflow run "Telegator macOS" --repo tokenator-team/telegator --ref main -f only_cache=false
```

- `only_cache=true` — только собрать библиотеки в кеш. Кеш привязан к хэшу
  `prepare.py`, поэтому после его изменения первая сборка — с библиотеками.
- Ключи — секреты репозитория `TDESKTOP_API_ID` / `TDESKTOP_API_HASH`; без них
  подставляются тестовые ключи Telegram (вход сильно ограничен).
- Все унаследованные workflow Telegram отключены через `gh workflow disable`.
  Не включай их и не удаляй их файлы (удаление = конфликты при обновлении).

## Запуск собранного клиента на Mac владельца

Клиент называется Telegator (bundle id `io.github.kdedushev.telegator`) и
ставится рядом с Telegram Desktop, не трогая его сессию. Все сборки на Mac —
локальные, облачные, отладочные — берут одну папку данных
`~/Library/Application Support/Telegator/`: там вход в аккаунт и
`telegator.json` (формат — `telegator/telegator_config.h`). Владелец входит
один раз, новая сборка и запуск двойным щелчком вход не теряют. Отладочная
сборка Telegram хранила бы данные рядом с приложением — метка `// Telegator`
в `logs.cpp`.

**У владельца ровно одна копия — `/Applications/Telegator.app` (Release).**
Новую сборку ставит только скрипт: собрать Release (раздел «Сборка»), затем

```bash
tools/telegator/install_mac.sh
```

Он закрывает Telegator, переносит (не копирует) `out/Release/Telegator.app`
в `/Applications`, убирает сборки из Launch Services и запускает. Копии в
`builds/`, на рабочем столе, в `/tmp` не делать (владелец 25.09.2026: копий
накопилось несколько).

Агенту для проверки своей правки — Debug из `out/Debug` (закрыв установленный),
после проверки обязательно вернуть владельцу его копию; `--restore` удаляет
`out/Debug/Telegator.app`, чтобы в поиске Spotlight не было второго Telegator
(следующая Debug-сборка создаёт его заново):

```bash
open -n ~/Projects/telegator-wt/dev/out/Debug/Telegator.app
tools/telegator/install_mac.sh --restore
```

- Второй Telegator с той же папкой не запускается, а открывает окно первого:
  чтобы проверить новую сборку, закрой работающую.
- `-workdir <папка>` — только для опыта на отдельном аккаунте: там свой вход,
  перед удалением папки — выйти из аккаунта.

Для ручной проверки сообщений — чат «Избранное», не живые собеседники.

## Раздача сотрудникам (macOS)

Собрать Release (раздел «Сборка»), затем

```bash
tools/telegator/package_mac.sh
```

Результат — `~/Projects/telegator-wt/dist/Telegator-<версия>-<коммит>.dmg`:
окно «перетащите в Программы» с подсказкой первого запуска. В пакет кладётся
`~/Projects/telegator-wt/package.json` (вне git, только раздел `panel`) —
`Contents/Resources/telegator.json`; свой `telegator.json` в папке данных
его перекрывает. Ключ журнала в пакет не кладётся: он у каждого компьютера
свой. Подпись — ad hoc (macOS один раз просит «Всё равно открыть»);
`TELEGATOR_SIGN_IDENTITY` — подпись сертификатом Developer ID. Сотрудник
входит в панель своим логином админки — `tk admin add-staff` меты.

## Раздача сотрудникам (Windows)

Облако: `gh workflow run telegator-win.yml --repo tokenator-team/telegator --ref main`
(Actions → «Telegator Windows»), затем `tools/telegator/fetch_win.sh` —
`~/Projects/telegator-wt/dist/Telegator-setup.exe`. Панель — секрет
`TELEGATOR_PANEL_CONFIG`, ложится рядом с exe; артефакт — 7z под ключом
`~/Projects/telegator-wt/artifact.key` (= секрет `TELEGATOR_ARTIFACT_KEY`).
Библиотеки — в кеше Actions; недособранные — перезапуск продолжает.
Бесплатный раннер — 16 ГБ: компилятор ограничен `/MP2` и подкачкой
(без этого C1060). Сборка Telegator — ~2,5 ч. Первый запуск — SmartScreen
«Подробнее → Выполнить в любом случае».

## Обновление с Telegram

```bash
git fetch upstream --tags
git rebase <новый тег или upstream/dev>
```

Конфликты — в местах `// Telegator` и в агентских файлах (`AGENTS.md`,
`CLAUDE.md`, `.agents/skills`). После rebase — сборка и проверка в клиенте.
`main` на GitHub не переписывать без явного согласия владельца.

`AGENTS.md` у нас — раздел «Telegator» и сжатая выжимка upstream: в его
конфликте оставь нашу версию и перенеси из `git diff <старая база> <новая> --
AGENTS.md` только правила, нужные почти в любой задаче; подробности агенты
читают в оригинале upstream.

## «Реквизиты»

Пункт меню сообщения — `telegator_requisites.*`, оформление —
`telegator_requisites_format.*`: перенос модуля админки
`tokenator-admin/src/apps/requisites` (там правила владельца и их тесты).
Поменять правила: сначала модуль и его тесты, затем эталоны, затем перенос
в C++, пока проверка не станет зелёной:

```bash
cd ~/Projects/tokenator/tokenator-admin
PYTHONPATH=src .venv/bin/python ~/.tokenator-konveyer/rekvizity-telegator/export_vectors.py
cd ~/Projects/telegator-wt/dev/Telegram
nice -n 10 cmake --build ../out --config Debug --target test_telegator_requisites
../out/Debug/test_telegator_requisites ~/.tokenator-konveyer/rekvizity-telegator/vectors.json
```

- Эталоны (`vectors.json`, «вход → ответ модуля», сравнение байт в байт,
  `entities` в единицах UTF-16) лежат вне репозитория: он публичный, а в
  тестах модуля — примеры владельца.
- Встроенные проверки теста — на выдуманных номерах: цифры не искажаются,
  пересланное — отказ, своё — правка, два телефона или два имени через
  «или» — выбор, подписи клиента («IBAN:», «Банк:») снимаются, имя — как
  написано («Мария И», «Иванов Д.С.»).
- Регулярные выражения модуля перенесены дословно; `\w \d \s \b` в
  `PyPattern()` переводятся в классы Python (сверено по всем символам
  Unicode 15.1, как у Python 3.13 модуля).

## Журнал действий

`telegator_journal.*` (24.09.2026): кто, с какого компьютера отправил, правил,
удалил, переслал, закрепил, поставил реакцию, нажал кнопку бота или пункт
панели. Все запросы к Telegram проходят через `MTP::Instance::sendRequest`
(метка `// Telegator`), удаление — через `Histories::deleteMessages`.
Локально — `<папка данных>/telegator_journal/*.jsonl`; на сервер — если в
`telegator.json` есть `"journal": {"url", "key"}`, пачками, недоставленное
ждёт в тех же файлах. Страница панели сообщает, кто вошёл:
`Telegator.setOperator(имя)`. Устройство — `telegator_device.json`.

## Гигиена — чтобы не копился мусор

- Для повторного использования сборки обычно подходит `~/Projects/telegator-wt/dev` с его `out/` (полная сборка ~30 минут, дальше минуты). Выбирай worktree по задаче: отдельный worktree допустим, когда он упрощает параллельную работу или изоляцию изменений. `git worktree move` с подмодулями не работает. После слияния в `main` удали ветку локально и на GitHub.
- **`builds/` не используется** — установленная копия одна (раздел «Запуск»).
  Нашёл старую сборку с `tdata/` или `workdir/` — сначала выйти в ней из
  аккаунта (иначе на сервере Telegram остаётся активная сессия), затем удалить.
- **Временные клоны** чужих форков и логи — в `/tmp`, удалить в конце задачи.
- **Документы:** `TELEGATOR.md` и `docs/telegator/ROADMAP.md` — основные. Новые планы и документы создавай, когда это помогает работе; сделанное переноси в «Сделано» ROADMAP, пометки `TEST-BUTTON-POC`/`// Telegator` не оставляй мёртвыми.
- **GitHub:** артефакты сборок живут 14 дней; кеш библиотек может удалиться после 7 дней без сборок. Если обычная сборка не проходит из-за отсутствующего кеша, запусти `only_cache=true` и повтори сборку.
