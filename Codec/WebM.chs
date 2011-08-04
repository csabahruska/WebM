{-#LANGUAGE ForeignFunctionInterface#-}
#include "webmutil.h"

module Codec.WebM (allocVP8,freeVP8,encodeFrame) where

import Codec.WebM.C2HS
import Control.Monad
import Data.ByteString
import Data.Word
import Foreign.ForeignPtr

{#pointer Context as VP8Codec foreign newtype#}
mkVP8Codec p = liftM VP8Codec $ newForeignPtr_ $ castPtr p

{#pointer Buffer foreign newtype#}
mkBuffer p = liftM Buffer $ newForeignPtr_ $ castPtr p

{#fun allocVP8 { `Int', `Int' } -> `VP8Codec' mkVP8Codec* #}
{#fun freeVP8 { withVP8Codec* `VP8Codec'  } -> `()' #}
{#fun rawFrameBuffer { withVP8Codec* `VP8Codec'  } -> `Buffer' mkBuffer* #}
{#fun encodeFrame as encodeFrameRaw { withVP8Codec* `VP8Codec', alloca- `Int' peekIntConv* } -> `Buffer' mkBuffer* #}

encodeFrame :: VP8Codec -> (Ptr Word8 -> IO ()) -> IO ByteString
encodeFrame vp8 fill = do
    b <- rawFrameBuffer vp8
    withBuffer b $ \a -> fill $ castPtr a
    (d,len) <- encodeFrameRaw vp8
    withBuffer d $ \src -> packCStringLen (castPtr src,len)