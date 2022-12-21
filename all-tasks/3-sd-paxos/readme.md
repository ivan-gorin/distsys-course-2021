# Single-Decree Paxos

Реализуйте RPC-сервис `Proposer` с единственным методом `Value Propose(Value input)`, который решает задачу консенсуса:

## Свойства

- _Agreement_ – все вызовы `Propose` возвращают одно и то же значение.
- _Validity_ – возвращать можно только одно из предложенных значений.
- _Termination_ – каждый вызов `Propose` завершается.

См. [SafetyChecker](./consensus/checker.cpp).

## Алгоритм

Для решения консенсуса используйте алгоритм _Single-Decree Paxos_ (далее – просто _Paxos_):

- [The Part-Time Parliament](https://lamport.azurewebsites.net/pubs/lamport-paxos.pdf)
- [Paxos Made Simple](https://lamport.azurewebsites.net/pubs/paxos-simple.pdf)
- [TLA+ Spec](https://github.com/tlaplus/Examples/blob/master/specifications/Paxos/Paxos.tla)
- [Псевдокод](https://pdos.csail.mit.edu/archive/6.824-2013/notes/paxos-code.html)

## Роли

В алгоритме _Paxos_ выделены следующие _роли_:

* _Proposer_ – координирует выбор значения, выполняет _Propose_.
* _Acceptor_ – голосует за (или отвергает) предложения, выдвигаемые _proposer_-ами.
* _Learner_ – опциональная роль, отслеживает отданные _acceptor_-ами голоса и определяет выбранное (_chosen_) значение.

Реализуйте эти роли в виде отдельных RPC-сервисов.

### Замечание

Алгоритм _Paxos_ не требует, чтобы _proposer_-ы и _acceptor_-ы находились на одних и тех же узлах.

Алгоритм требует, чтобы набор _acceptor_-ов был зафиксирован (они образуют кворумы), но допускает произвольное число _proposer_-ов.

В нашем случае _acceptor_-ы – это узлы системы, а каждый RPC `Proposer.Propose` – это новый _proposer_.

## Кворум

Каждая фаза алгоритма может завершиться тремя исходами:

1) Кворум успешно собрался (например, большинство _acceptor_-ов ответили _Promise_-ом на _Prepare_)
2) Кворум провалился (слишком много _acceptor_-ов отвергли _Prepare_)
3) Кворум завис (из-за партишена или отказа половины узлов)

Вероятно, вам будет недостаточно функциональности существующего комбинатора `Quorum`.

Реализуйте собственный кворумный комбинатор по аналогии с `All` / `FirstOf` / `Quorum` из библиотеки `await`.

Чтобы не дублировать код, сделайте этот комбинатор обобщенным (параметризуемым фазой алгоритма).

Подумайте над гарантиями _at-least-once_ / _at-most-once_ в RPC.

## Exponential backoff

Используйте exponential backoff и рандомизацию для предотвращения лайвлока между _proposer_-ами.

В фреймворке мы работаем с абстрактными единицами времени (_jiffies_), которые могут иметь разный смысл в зависимости от движка или параметров симуляции.

Чтобы не подстраиваться под конкретные параметры симуляции, для ретраев используйте следующие поля из конфигурации:

_Поле_ | _Тип_ | _Описание_
 --- | --- | ---
`paxos.backoff.init` | `int64_t` | Начальное время ожидания
`paxos.backoff.max` | `int64_t` | Максимальное время ожидания
`paxos.backoff.factor` | `int64_t` | Множитель 

## Advice

Если кворум первой или второй фазы _Paxos_ провалился, _proposer_ должен пройти обе фазы заново со свежим _proposal number_-ом _n_.

Подумайте, как _acceptor_-ы могут помочь _proposer_-у с выбором нового _n_.

## Бонусное задание

Прочтите статью Лэмпорта [Paxos Made Simple](https://lamport.azurewebsites.net/pubs/paxos-simple.pdf) и самостоятельно оцените истинность следующего утверждения:

_The Paxos algorithm for implementing a fault-tolerant distributed system
has been regarded as difficult to understand, perhaps because the original
presentation was Greek to many readers. In fact, it is among the simplest and most obvious of distributed algorithms._
