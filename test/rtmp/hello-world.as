// makeswf -v 7 -s 200x150 -r 1 -o hello-world.swf hello-world.as

nc = new NetConnection ();
nc.onStatus = function (info) {
  trace (info.code);
  onEnterFrame = function () {
    nc.call ("ping");
    delete onEnterFrame;
  };
};

nc.connect ("rtmp://localhost/test");
nc.pong = function () {
  trace ("pong");
  getURL ("fscommand:quit", "");
};

