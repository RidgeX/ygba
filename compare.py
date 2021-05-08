import pyautogui
import pyperclip
import time

def get_lines():
	#time.sleep(0.1)  # Wait for copy to complete
	lines = pyperclip.paste().split('\r\n')
	line_r0 = [line for line in lines if 'r0:' in line][-1]
	line_r4 = [line for line in lines if 'r4:' in line][-1]
	line_r8 = [line for line in lines if 'r8:' in line][-1]
	line_r12 = [line for line in lines if 'r12:' in line][-1]
	line_cpsr = [line for line in lines if 'cpsr:' in line][-1]
	return line_r0, line_r4, line_r8, line_r12, line_cpsr

def main():
	while pyautogui.position() != (0, 0):
		debugger = pyautogui.getWindowsWithTitle('Debugger')[0]
		cmd = pyautogui.getWindowsWithTitle('cmd.exe - gba')[0]

		pyautogui.click(debugger.centerx, debugger.centery)
		pyautogui.hotkey('ctrl', 'a')
		pyautogui.hotkey('ctrl', 'c')
		mgba_lines = get_lines()

		pyautogui.click(cmd.centerx, cmd.centery)
		pyautogui.hotkey('ctrl', 'a')
		pyautogui.hotkey('ctrl', 'c')
		ygba_lines = get_lines()

		if not mgba_lines == ygba_lines: return

		pyautogui.click(debugger.left + 32, debugger.top + debugger.height - 32)
		pyautogui.write(['n', 'enter'])
		pyautogui.click(cmd.centerx, cmd.centery)
		pyautogui.write(['enter'])

if __name__ == '__main__':
	main()
