#ifndef FIOS2AC_H
#define FIOS2AC_H

#include <psp2/io/stat.h>
#include <psp2/types.h>

typedef SceInt32 SceFiosFH;
typedef SceUInt64 SceFiosDate;
typedef SceInt64 SceFiosOffset;
typedef SceInt64 SceFiosSize;

typedef struct SceFiosStat {
	SceFiosOffset fileSize;
	SceFiosDate accessDate;
	SceFiosDate modificationDate;
	SceFiosDate creationDate;
	SceUInt32 statFlags;
	SceUInt32 reserved;
	SceInt64 uid;
	SceInt64 gid;
	SceInt64 dev;
	SceInt64 ino;
	SceInt64 mode;
} SceFiosStat;

typedef enum SceFiosWhence {
	SCE_FIOS_SEEK_SET = 0,
	SCE_FIOS_SEEK_CUR = 1,
	SCE_FIOS_SEEK_END = 2
} SceFiosWhence;

SceInt32 sceFiosStatSync(const ScePVoid attr, const SceName path, SceFiosStat* stat);
SceInt32 sceFiosFHOpenSync(const ScePVoid attr, SceFiosFH* fh, const SceName path, const ScePVoid params);
SceFiosSize sceFiosFHReadSync(const ScePVoid attr, SceFiosFH fh, ScePVoid data, SceFiosSize size);
SceInt32 sceFiosFHCloseSync(const ScePVoid attr, SceFiosFH fh);
SceFiosOffset sceFiosFHSeek(SceFiosFH fh, SceFiosOffset offset, SceFiosWhence whence);

#endif
