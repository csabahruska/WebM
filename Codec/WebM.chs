{-#LANGUAGE ForeignFunctionInterface#-}
#include "webmutil.h"

module Codec.WebM where

import C2HS
import Control.Monad
import Foreign.ForeignPtr

{#pointer Context as VP8Codec foreign newtype#}
mkVP8Codec p = liftM VP8Codec $ newForeignPtr_ $ castPtr p

{#pointer Buffer foreign newtype#}
mkBuffer p = liftM Buffer $ newForeignPtr_ $ castPtr p

{#fun allocVP8 { `Int', `Int' } -> `VP8Codec' mkVP8Codec* #}
{#fun freeVP8 { withVP8Codec* `VP8Codec'  } -> `()' #}
{#fun rawFrameBuffer { withVP8Codec* `VP8Codec'  } -> `Buffer' mkBuffer* #}
{#fun encodeFrame { withVP8Codec* `VP8Codec', alloca- `Int' peekIntConv* } -> `Buffer' mkBuffer* #}
