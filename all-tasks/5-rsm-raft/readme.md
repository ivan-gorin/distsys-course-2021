# Raft

## Пролог

_After struggling with Paxos ourselves, we set out to find a new consensus algorithm that could provide a better foundation for system building and education_.

Diego Ongaro and John Ousterhout.

## Задача

Реализуйте отказоустойчивый линеаризуемый RSM с помощью протокола репликации Raft.

## Алгоритм

- https://raft.github.io
- [In Search of an Understandable Consensus Algorithm](https://raft.github.io/raft.pdf)
- [Consensus: Bridging Theory and Practice](https://github.com/ongardie/dissertation)
- [Лекция](https://www.youtube.com/watch?v=YbZ3zDzDnrw), [слайды](https://ongardie.net/static/raft/userstudy/raft.pdf)
- [Спецификация TLA+](https://github.com/ongardie/raft.tla/blob/master/raft.tla)
- [Raft-dev Google Group](https://groups.google.com/g/raft-dev)

## Реализация

### Референсы

- [LogCabin](https://github.com/logcabin/logcabin)
- [Implementing Raft](https://eli.thegreenplace.net/2020/implementing-raft-part-0-introduction/)
- [Students' Guide to Raft](https://thesquareplanet.com/blog/students-guide-to-raft/)

### Роли и коммуникация

#### Multi-Paxos

В Multi-Paxos каждая реплика сочетает роли и _proposer_-а, и _acceptor_-а, так что _proposer_ отправляет сообщения в том числе и самому себе, т.е. локальному _acceptor_-у.

#### Raft

В Raft у каждой реплики в любой момент времени только одна роль (лидер / кандидат / фолловер), и вся коммуникация только внешняя.

### Репликация и кворумы

#### Multi-Paxos

В наивной реализации Multi-Paxos команды коммитятся по одной, и независимо друг от друга. 

Для коммита команды в слот proposer должен последовательно пройти через две фазы Single-Decree Paxos, на каждой из фаз собрать кворум ответов реплик.

#### Raft

В отличие от Multi-Paxos, алгоритм Raft не работает с отдельными слотами лога и не собирает кворумы явно (т.е. в реализации нет шага, где алгоритм _дожидается_ кворума). 

Лидер в Raft реплицирует лог 
- фрагментами (см. `entries` в сообщении `AppendEntries`) 
- независимо, в своем собственном темпе для каждого из фолловеров (см. `nextIndex` в спецификации алгоритма).

Для каждого фолловера лидер отслеживает прогресс репликации в виде `matchIndex`. Сопоставляя `matchIndex`-ы фолловеров, лидер понимает, что префикс лога лежит на кворуме реплик, и тогда двигает вперед `commitIndex`.

### Декомпозиция и concurrency

#### Multi-Paxos

О наивном Multi-Paxos удобно думать как о серии независимых консенсусов.

Конвейер, через который проходит отдельная команда, удобно выкладывается
в набор файберов:
- Общий файбер, который выкладывает команды в лог
- Параллельные файберы для консенсуса в отдельных слотах
- Общий файбер, который применяет зафиксированные в логе команды к автомату

Модуль выбора лидера перпендикулярен репликации и может быть реализован в виде отдельного модуля.

#### Raft

Raft гораздо более монолитен и на уровне кода декомпозируется хуже.

##### Коллбэки и файберы

С одной стороны, удобно думать про реплику в Raft как про автомат с атомарными переходами по внешним конкурентным событиям (RPC, таймеры).
В коде такой автомат было бы органично реализовать на коллбэках.

С другой стороны, в алгоритме достаточно последовательных активностей с синхронными RPC внутри (цикл репликации, сбор голосов при выборе лидера). Такие активности было бы удобно представить в виде файберов.

Проблема в том, что два этих подхода не комбинируются.

##### Гибридный подход

Вам предлагается использовать не слишком изящный, зато довольно простой и прямолинейный гибридный подход, описанный в [Implementing RAFT](https://eli.thegreenplace.net/2020/implementing-raft-part-0-introduction/):

- На любую конкурентную активность (таймеры, репликация лога, сбор голосов в фазе выбора лидера) заводится отдельный файбер.
- Каждый файбер привязывается к своему терму и роли.
- Все состояние реплики (`currentTerm`, `log` и т.д.) защищается одним мьютексом.
- Любая блокирующая операция (RPC, ожидание на канале) выполняется с отпущенным мьютексом.
- После повторного захвата мьютекса (например, после RPC) нужно перепроверять, что действие все еще актуально (терм или роль реплики не изменилась).

##### Mutex vs Strand

В качестве альтернативы мьютексу ([`Mutex`](https://gitlab.com/Lipovsky/await/-/blob/master/await/executors/mutex.hpp)) можно использовать асинхронный мьютекс – экзекутор [`Strand`](https://gitlab.com/Lipovsky/await/-/blob/master/await/executors/strand.hpp).

##### LogCabin vs etcd/raft

Описанный выше подход используется в референсной реализации Raft, написанной авторами алгоритма – [LogCabin](https://github.com/logcabin/logcabin).

Альтернативный подход выбран в [etcd/raft](https://github.com/etcd-io/etcd/tree/main/raft): алгоритм Raft отделен от рантайма (concurrency, RPC, время) и реализован как [однопоточный автомат](https://github.com/etcd-io/etcd/blob/8f17652c6096757feaf68973161508730ba2fa57/raft/raft.go#L847).

### Crash recovery

Переменные `nextIndex`, `matchIndex` и `commitIndex` не хранятся на диске и сбрасываются при рестарте реплики.

Подумайте, как реплика восстанавливает эти поля после рестарта.

### Ticker + Select

Лидер в Raft отправляет фолловерам `AppendEntries` по двум событиям:
- Тик таймера (`AppendEntries` в этом случае служит хартбитом),
- Появление новых команд в логе.

Для реализации [тикера](https://gobyexample.com/tickers) используйте канал ([`await::fibers::Channel<T>`](https://gitlab.com/Lipovsky/await/-/blob/master/await/fibers/sync/channel.hpp)) единичной емкости + неблокирующую операцию `TrySend`.

Мультиплексировать события из разных каналов можно с помощью [`await::fibers::Select`](https://gitlab.com/Lipovsky/await/-/blob/master/await/fibers/sync/select.hpp), аналогично [`select`-у из Golang](https://gobyexample.com/select).

## Raft vs Multi-Paxos

- [Instructors' Guide to Raft](https://thesquareplanet.com/blog/instructors-guide-to-raft/)
- [Paxos vs Raft: Have we reached consensus on distributed consensus?](https://arxiv.org/abs/2004.05074)
