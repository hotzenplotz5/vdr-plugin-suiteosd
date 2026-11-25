# Shared VDR-Suite genre registry

The two headers in this directory are unmodified copies of
`core/vdr/include/CanonicalGenreRegistry.h` and `GenreClassification.h`
from hotzenplotz5/vdr-suite commit `52c67da2c74a2757785ded3eb3bccecc703756ce`.
Keep them synchronized with that upstream registry when updating genre aliases.
The native OSD uses its German canonical labels for local TVScraper genre
grouping; it does not create another classification database or call HTTP.
Unknown provider labels remain available. The OSD keeps its existing
"Ohne Genre" label for unclassified recordings.
