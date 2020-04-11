TTS
===

Transient Time Series, lightweight in-memory time-series database. Rudimental
TSDB without persistence, allow to create named time-series and store points
with nanosecods precision, currently supports only basic operations:

- `CREATE timeseries-name [retention]`
- `DELETE timeseries-name`
- `ADD timeseries-name timestamp|* value [label value ..] - ..`
- `QUERY timeseries-name [>|<|range] start_timestamp [end_timestamp] [avg value]`

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
