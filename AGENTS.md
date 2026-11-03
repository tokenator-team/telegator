# Telegator — форк Telegram Desktop

- Внутренний клиент macOS/Windows; наши функции находятся в `Telegram/SourceFiles/telegator/`, планы — `docs/telegator/ROADMAP.md`; публичный `main` — `tokenator-team/telegator`, апстрим — `upstream/dev`.
- Логику, строки, стили и состояние держать в `telegator/`; в чужих исходниках Telegram оставлять только точку вызова 1–3 строки с `// Telegator`. Новые исходники подключать через `Telegram/cmake/telegator.cmake`.
- Клиентские действия только сообщают серверу владельца; бизнес-решение и исполнение остаются на сервере. Исключение владельца (23.09.2026): оформление, справочник банков, отправка и закреп «Реквизитов» — в клиенте.
- Не хранить API-секреты и бизнес-специфику владельца в репозитории; не сохранять сообщения с таймером самоуничтожения.
- Проверка — `make check`; сборка Debug описана в `TELEGATOR.md` и `docs/building-*.md`. Для macOS при перенастройке существующего `out/` запускать `env -u QT cmake -S . -B out`.
- У владельца одна установленная копия `/Applications/Telegator.app`; ставить и восстанавливать её только `tools/telegator/install_mac.sh`. Не проверять сообщения на живых собеседниках — использовать «Избранное».
- В `main` вливать только `python3 ~/Projects/tokenator/scripts/merge.py telegator <ветка>`; прямой push и переписывание GitHub `main` запрещены.
- Справочные процедуры сборки, установки и обновления — `TELEGATOR.md`; механические требования — `REVIEW.md`.
- Очередь `ai-tdesktop` и upstream-навыки запускать только по явной просьбе; upstream-инструкции читать через `git show $(git merge-base HEAD upstream/dev):AGENTS.md`.
