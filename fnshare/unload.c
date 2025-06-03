void unload()
{
  _asm {
exit_ret:
  }

  {
  void *exit_ret_ptr = &&exit_ret;
  }

}
