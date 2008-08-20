package {
  import flash.display.Sprite;
  import flash.system.fscommand;

  public class coercion extends Sprite {

    public function test (a:String, b:int) {
      trace (a + b)
    }

    public function coercion () {
      var x = 4;
      test (x, 2)
      fscommand ("quit")
    }
  }
}
