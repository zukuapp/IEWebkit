/* Exercise the exact automation decoding used by the IE event adapter. */
#include "../host/navigation_bho.cpp"
int main() {
  VARIANT value, first, second, cycle, post;
  VariantInit(&value);
  VariantInit(&first);
  VariantInit(&second);
  VariantInit(&cycle);
  VariantInit(&post);
  first.vt = VT_VARIANT | VT_BYREF;
  first.pvarVal = &value;
  second.vt = VT_VARIANT | VT_BYREF;
  second.pvarVal = &first;
  cycle.vt = VT_VARIANT | VT_BYREF;
  cycle.pvarVal = &cycle;
  bool pass = empty(&second) && unbox(&second) == &value && !unbox(&cycle) &&
              !empty(&cycle);
  value.vt = VT_BSTR;
  value.bstrVal = SysAllocString(L"Authorization: must not be dropped");
  pass = pass && value.bstrVal && !empty(&second);
  VariantClear(&value);
  post.vt = VT_ARRAY | VT_UI1;
  post.parray = SafeArrayCreateVector(VT_UI1, 0, 1);
  pass = pass && post.parray && !empty(&post);
  VariantClear(&post);
  HANDLE file =
      CreateFileA("C:\\ZUKUQA\\NAVVALUE.LOG", GENERIC_WRITE, FILE_SHARE_READ,
                  NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return 2;
  const char *text = pass ? "PASS nested IE variants, cyclic rejection, "
                            "nonempty headers and POST rejection\r\n"
                          : "FAIL navigation variant decoding\r\n";
  DWORD written;
  WriteFile(file, text, lstrlenA(text), &written, NULL);
  CloseHandle(file);
  return pass ? 0 : 1;
}
