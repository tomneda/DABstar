# Configuration

The **Configuration and Control** window holds the global settings of DABstar.

![The configuration and control window](../../res/for_readme/configuration.png)

Nearly every element of this window carries a tooltip with a detailed explanation, so hover over the
control in question for the full description.

One setting is referenced from elsewhere in this manual: the **home coordinates**, which are needed
to show location, distance and direction to the received transmitters, see
[TII and Map](70-tii-and-map.md).

## Where the settings are stored

DABstar keeps all persistent data in `~/.config/dabstar/`:

| File                | Content                                            |
|---------------------|------------------------------------------------------|
| `settings03.ini`    | All program settings                               |
| `servicelist04.db`  | The [service list](30-service-list.md) database    |
| `ensemblelist04.db` | The [ensemble list](40-ensemble-list.md) database  |

The trailing number of the database files is a database version number. It is raised when the
database layout changes, so an older version cannot be read by mistake.
