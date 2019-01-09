using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;

namespace WebAPI.Controllers
{
    [Route("api/[controller]")]
    public class ValuesController : Controller
    {
        private const int Pin16 = 16;
        private const int Pin17 = 17;

        private Task _writeHighTask;
        private Task _writeLowTask;

        // GET api/values
        [HttpGet]
        public IEnumerable<string> Get()
        {
            
            _writeHighTask = Task.Run(async () =>
            {
                if (_writeLowTask != null)
                {
                    await _writeLowTask;
                }

                WiringPi.GPIO.digitalWrite(
                    Pin16,
                    (int)WiringPi.GPIO.GPIOpinvalue.High);
                
            });
            return new string[] { "value1", "value2" };
        }

        // GET api/values/5
        [HttpGet("{id}")]
        public string Get(int id)
        {
            _writeLowTask = Task.Run(async () =>
            {
                if (_writeHighTask != null)
                {
                    await _writeHighTask;
                }

                WiringPi.GPIO.digitalWrite(
                    Pin16,
                    (int)WiringPi.GPIO.GPIOpinvalue.Low);
            });
           
            return "value";
        }

        // POST api/values
        [HttpPost]
        public void Post([FromBody]string value)
        {
        }

        // PUT api/values/5
        [HttpPut("{id}")]
        public void Put(int id, [FromBody]string value)
        {
        }

        // DELETE api/values/5
        [HttpDelete("{id}")]
        public void Delete(int id)
        {
        }
    }
}
