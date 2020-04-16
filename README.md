TTS
===

Transient Time Series, lightweight in-memory time-series database. Rudimental
TSDB without persistence, allow to create named time-series and store points
with nanosecods precision, currently supports only basic operations:

- `CREATE timeseries-name [retention]`
- `DELETE timeseries-name`
- `ADD timeseries-name timestamp|* value [label value ..] - ..`
- `MADD timeseries-name timestamp|* value timeseries-name timestamp|* value ..`
- `QUERY timeseries-name [>|<|RANGE] start_timestamp [end_timestamp] [AVG value]`

Retention is currently ignored as argument, `ADD` supports multiple points
sharing the same timestamp separated by `-` character.
`QUERY` offers some simple aggregation, a mean on time-window and ranges. To be
improved.

Fun-fueled project **not suitable** for production uses.

## Compilation

To compile the project just need make

```sh
$ make
```

The CLI as well

```sh
$ make tts-cli
```

## Some more details

Under the hood the basics are pretty simple, there're 2 main data-structures
involved:

- *dynamic arrays*
- *hashmap*

Once a timeseries is created, the trend it represents (the points) is stored in
a pair of vectors, one dedicated to timestamps and the other to the records,
which are comprised of a `long double` typed value and a list of optional
labels. These vectors maps directly one-another and they're sorted by design,
allowing to leverage binary search to query ranges and single values.

Each timeseries is stored into a global index represented by a general hashmap.
A reference to the labels is also inserted into a multilevel hashmap related to
the timeseries itself, the mapping is based on the labels nominative and their
value. Shortly speaking each label name defines an hashmap, and each label
value define a sub-hashmap as entry of the label hashmap itself. This way it's
fairly easy to index labels and use them to filter out results.
