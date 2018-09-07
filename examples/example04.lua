  local mtmsg = require("mtmsg")
  local listener = mtmsg.newlistener()
  local _, err = pcall(function() listener:nextmsg() end)
  assert(err:match(mtmsg.error.no_buffers))
