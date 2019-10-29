from .color import Color
import random
import time

class Animation:
    def on_listen(self, provider):
        provider.clear_all()
        groups = int(provider.run_params.num_leds / 3)
        while provider.run_params.curr_state == "ON_LISTEN":
            for i in range(0, 3):
                for g in range(0, groups):
                    if provider.run_params.curr_state != "ON_LISTEN":
                        break
                    
                    color = self.remap_color(provider.run_params.animation_color.listen, provider.run_params.max_brightness)
                    provider.set_color(g * 3 + i, color)
                provider.refresh()
                self.delay_on_state(80, "ON_LISTEN", provider)
                provider.clear_all()
                self.delay_on_state(80, "ON_LISTEN", provider)
            provider.clear_all()
    
    def delay_on_state(self, ms, state, provider):
        for i in range(0, ms):
            if provider.run_params.curr_state != state:
                break
            time.sleep(0.001)
        
    def remap_color(self, color, brightness):
        r = int(color.r * brightness / 255);
        g = int(color.g * brightness / 255);
        b = int(color.b * brightness / 255);
        return Color(r, g, b);
    
    def on_speak(self, provider):
        curr_bri = 0;
        provider.clear_all()
        step = int(provider.run_params.max_brightness / 20)
        
        while provider.run_params.curr_state == "ON_SPEAK":
            for curr_bri in range(0, provider.run_params.max_brightness, step):
                for j in range(0, provider.run_params.num_leds):
                    if provider.run_params.curr_state != "ON_SPEAK":
                        break
                    
                    provider.set_color(j, self.remap_color(provider.run_params.animation_color.speak, curr_bri))
                provider.refresh()
                self.delay_on_state(20, "ON_SPEAK", provider)
            
            curr_bri = provider.run_params.max_brightness
            
            for curr_bri in range(provider.run_params.max_brightness, 0, -step):
                for j in range(0, provider.run_params.num_leds):
                    if provider.run_params.curr_state != "ON_SPEAK":
                        break
                
                    provider.set_color(j, self.remap_color(provider.run_params.animation_color.speak, curr_bri))
                provider.refresh()
                self.delay_on_state(20, "ON_SPEAK", provider)
            provider.clear_all()
            provider.refresh()
            self.delay_on_state(200, "ON_SPEAK", provider)
        provider.clear_all()
        
    def on_idle(self, provider):
        curr_bri = 0;
        provider.clear_all()
        
        step = int(provider.run_params.max_brightness / 20)
        
        while provider.run_params.curr_state == "ON_IDLE":
            self.delay_on_state(2000, "ON_IDLE", provider)
            provider.clear_all()
            led = random.randint(0, provider.run_params.num_leds - 1)
            
            for curr_bri in range(0, provider.run_params.max_brightness, step):
                if provider.run_params.curr_state != "ON_IDLE":
                    break
            
                provider.set_color(led, self.remap_color(provider.run_params.animation_color.idle, curr_bri))
                provider.refresh()
                self.delay_on_state(100, "ON_IDLE", provider)
            
            curr_bri = provider.run_params.max_brightness
            
            for curr_bri in range(provider.run_params.max_brightness, 0, -step):
                if provider.run_params.curr_state != "ON_IDLE":
                    break
                
                provider.set_color(led, self.remap_color(provider.run_params.animation_color.idle, curr_bri))
                provider.refresh()
                self.delay_on_state(100, "ON_IDLE", provider)
            
            provider.set_color(led, Color(0, 0, 0))
            provider.refresh()
            self.delay_on_state(3000, "ON_IDLE", provider)
        
        provider.clear_all()
    
