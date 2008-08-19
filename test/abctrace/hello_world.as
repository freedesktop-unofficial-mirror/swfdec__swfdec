package {
  import flash.display.Sprite;
  import flash.system.fscommand;

  public class hello_world extends Sprite {

    public function hello_world () {
      trace ("Hello World");
      trace ("Hello", "World");
      fscommand ("quit");
    }
  }
}
